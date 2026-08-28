#include "fft/data.h"
#include "fft/main_sound.h"
#include "fft/world.h"
#include "psx/pad.h"

extern world_order_menu_entry_t g_world_formation_panel_windows[];

/*
 * Run the formation Set Ability menu for the selected unit.
 *
 * Return 0 when canceled and 1 while active. With the list closed the cursor
 * picks an ability slot and opens its candidate list (world_ability_find_unit_abilities for the
 * secondary skillset, world_ability_build_equippable_rsm_list otherwise);
 * with it open, the chosen ability is written to the slot and the Move+N
 * stat preview is refreshed.
 *
 * `cursor` is only assigned while the list has entries, as in the target,
 * which compares its stale register value when the list is empty. The inner
 * `(u16)` casts keep the Move+N range checks in unsigned-short arithmetic
 * (`li 0xfe1a` / `addu` / `andi 0xffff`).
 */
s32 world_formation_run_set_ability_menu(void) {
    s32 unused[2]; /* never referenced; reproduces the 0x40 frame */
    s16 x;
    s16 y;
    s16 lock_state;
    s8 slot;
    s32 current;
    s32 cursor;
    s32 value; /* description base, list text section, then the first support-change flag */
    s16 selected;
    s32 busy;

    if (g_world_set_ability_unit_index != g_world_formation_selected_unit_index) {
        g_world_set_ability_initialized = 0;
        g_world_set_ability_unit_index
            = ((world_menu_halfword_view_t*)&g_world_formation_selected_unit_index)->bytes[0];
    }
    if (g_world_set_ability_initialized == 0) {
        world_gfx_clear_sprite_slots();
        g_world_set_ability_slot = -1;
        g_world_set_ability_initialized = 1;
        g_world_set_ability_cursor = 0xFF;
        g_world_set_ability_list_close_pending = 0;
        D_8018D18A = 0;
        g_world_set_ability_list_open = 0;
        g_world_formation_unit_cycle_mode = 2;
        g_world_set_ability_show_stat_preview = 0;
    }
    if (g_world_input_primary_repeat & PSX_PAD_CROSS) {
        g_world_menu_sound_effect_id = MAIN_SFX_CANCEL;
        if (g_world_set_ability_list_open == 0) {
            world_thread_resume(0xF);
            g_world_set_ability_initialized = 0;
            world_menu_reset_selection_results();
            return 0;
        }
        g_world_set_ability_list_close_pending = 1;
    }
    lock_state = world_formation_lock_equipment_and_abilities(g_world_formation_selected_unit_index);
    if (lock_state == 1) {
        lock_state = 0;
    }
    if (lock_state != 0) {
        world_menu_toggle_preview_stats_window(0);
        g_world_menu_description_text_id = 0;
        if (g_world_input_primary_repeat & PSX_PAD_CIRCLE) {
            g_world_menu_sound_effect_id = MAIN_SFX_INVALID;
            return 1;
        }
        return 1;
    }
    if (g_world_set_ability_list_open == 0) {
        slot = g_world_set_ability_slot;
        current = ((world_formation_unit_ability_slots_t*)
                       g_world_formation_unit_pointers[g_world_formation_selected_unit_index])
                      ->ability_slots[slot];
        g_world_formation_unit_cycle_mode = 2;
        g_world_menu_description_text_id = current;
        if (current == 0) {
            g_world_menu_description_text_id = -1;
        } else if (slot >= 2) {
            g_world_menu_description_text_id = current + 0x7800;
        } else {
            g_world_menu_description_text_id = current + 0x9800;
        }
        slot = world_menu_step_cursor_with_sound(4, 4, g_world_input_secondary_repeat, 6) + 1;
        g_world_set_ability_slot = slot;
        if (g_world_input_primary_repeat & PSX_PAD_CIRCLE) {
            g_world_menu_sound_effect_id = MAIN_SFX_CONFIRM;
            if (slot == 1) {
                g_world_set_ability_entry_count
                    = world_ability_find_unit_abilities(g_world_formation_selected_unit_index,
                        g_world_formation_unit_pointers[g_world_formation_selected_unit_index]->job_id, 0,
                        g_world_menu_entry_ids, slot);
            } else {
                g_world_set_ability_entry_count = world_ability_build_equippable_rsm_list(
                    g_world_formation_selected_unit_index, slot - 1, g_world_menu_entry_ids);
            }
            world_script_set_vsync_mode_and_event_speed(2);
            g_world_formation_unit_cycle_mode = 1;
            g_world_set_ability_list_open = 1;
            world_thread_suspend(0xF);
            g_world_ability_panel_thread_params.style = 1;
            g_world_set_ability_cursor = 0xFF;
            world_gfx_get_sprite_slot(g_world_set_ability_slot + 4, &x, &y, g_world_menu_entry_ids);
            value = 0xE;
            if (g_world_set_ability_slot == 1) {
                value = 0x16;
            }
            world_menu_init_scrollable_list(g_world_menu_entry_ids, x, y, (u32)g_world_text_section_pointers[value]);
        }
    } else {
        if (g_world_menu_option_count != 0) {
            cursor = g_world_menu_cursor_position;
            value = 0x7800;
            if (g_world_set_ability_slot == 1) {
                value = 0x9800;
            }
            g_world_menu_description_text_id = value + (((u16*)g_world_menu_entry_ids)[cursor] & 0x3FF);
            if (g_world_input_primary_repeat & PSX_PAD_CIRCLE) {
                selected = ((u16*)g_world_menu_entry_ids)[cursor];
                if (selected & 0x4000) {
                    world_text_show_message_and_play_sound(0xD81D, 0x30);
                } else {
                    value = world_ability_is_equip_support_change(g_world_formation_selected_unit_index, selected);
                    world_formation_set_unit_ability_slot(g_world_formation_selected_unit_index,
                        g_world_set_ability_slot, selected,
                        value
                            | world_ability_is_equip_support_change(g_world_formation_selected_unit_index,
                                ((world_formation_unit_ability_slots_t*)
                                        g_world_formation_unit_pointers[g_world_formation_selected_unit_index])
                                    ->ability_slots[g_world_set_ability_slot]));
                    g_world_menu_sound_effect_id = MAIN_SFX_EQUIP;
                    g_world_ability_menu_slots_changed = 1;
                    g_world_set_ability_list_close_pending = 1;
                    world_formation_stage_selected_unit();
                }
            }
        } else {
            g_world_menu_description_text_id = -1;
            if (g_world_input_primary_repeat & PSX_PAD_CIRCLE) {
                if (g_world_set_ability_slot != 1) {
                    world_text_show_message_and_play_sound(0xC006, 0x30);
                } else {
                    g_world_menu_sound_effect_id = MAIN_SFX_INVALID;
                }
            }
        }
        if (g_world_set_ability_cursor != cursor) {
            g_world_set_ability_cursor = cursor;
            if ((u16)((u16)g_world_menu_entry_ids[cursor] - ABILITY_ID_MOVEMENT_MOVE_PLUS_1) < 6
                || (u16)((u16)((world_formation_unit_ability_slots_t*)
                                   g_world_formation_unit_pointers[g_world_formation_selected_unit_index])
                             ->ability_slots[g_world_set_ability_slot]
                       - ABILITY_ID_MOVEMENT_MOVE_PLUS_1)
                    < 6) {
                /* The target passes both ability ids without the definition's s16 narrowing. */
                ((void (*)(world_item_stat_detail_t*, s32, s32))world_ability_find_stat_changes_due_to_equipped)(
                    &g_world_item_preview_stat_detail,
                    ((world_formation_unit_ability_slots_t*)
                            g_world_formation_unit_pointers[g_world_formation_selected_unit_index])
                        ->ability_slots[g_world_set_ability_slot],
                    g_world_menu_entry_ids[g_world_menu_cursor_position]);
                g_world_preview_stats_thread_params.redraw_request = 1;
                g_world_set_ability_show_stat_preview = 1;
            } else {
                g_world_set_ability_show_stat_preview = 0;
            }
        }
        /* The target passes a0 = 0 to the argument-less world_thread_is_task_active. */
        world_menu_run_script_with_palette_mode(g_world_set_ability_window_script, g_world_input_primary_repeat,
            ((s32 (*)(s32))world_thread_is_task_active)(0));
    }
    if (g_world_set_ability_list_close_pending != 0) {
        world_menu_set_window_scale_step(0);
        g_world_ability_panel_thread_params.style = 0;
        g_world_set_ability_list_open = 0;
        g_world_formation_unit_cycle_mode = 2;
        world_script_set_vsync_mode_and_event_speed(0);
        g_world_set_ability_show_stat_preview = 0;
        g_world_formation_panel_windows[0].enabled = 1;
        world_gfx_set_sprite_slot(g_world_set_ability_slot + 4, g_world_menu_cursor_position,
            g_world_menu_scroll_offset, (u16*)g_world_menu_entry_ids);
        world_thread_resume(0xF);
        g_world_set_ability_list_close_pending = 0;
    }
    busy = 0;
    world_menu_toggle_preview_stats_window(g_world_set_ability_show_stat_preview);
    g_world_set_ability_cursor_point.y = g_world_set_ability_slot * 16 + 0x90;
    world_menu_set_draw_priority(0x28);
    /* The target passes a0 = 0 to the argument-less world_thread_is_task_active. */
    if (((s32 (*)(s32))world_thread_is_task_active)(0) != 0 || g_world_set_ability_list_open != 0) {
        busy = 1;
    }
    world_menu_draw_animated_cursor(&g_world_set_ability_cursor_point, &g_world_set_ability_cursor_anim, busy);
    return 1;
}
