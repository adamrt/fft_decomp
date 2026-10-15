#include "fft/thread.h"
#include "fft/world.h"
#include "psx/pad.h"

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

    if (g_world_set_ability_unit_index != g_unit_selected_index) {
        g_world_set_ability_initialized = 0;
        g_world_set_ability_unit_index = (u8)g_unit_selected_index;
    }
    if (g_world_set_ability_initialized == 0) {
        menu_clear_selection_records();
        g_world_set_ability_slot = -1;
        g_world_set_ability_initialized = 1;
        g_world_set_ability_cursor = 0xFF;
        g_world_set_ability_list_close_pending = 0;
        D_8018D18A = 0;
        g_world_set_ability_list_open = 0;
        g_world_formation_unit_cycle_mode = 2;
        g_world_set_ability_show_stat_preview = 0;
    }
    if (g_menu_input_primary_repeat & PSX_PAD_CROSS) {
        g_sound_queued_effect_id = MAIN_SFX_CANCEL;
        if (g_world_set_ability_list_open == 0) {
            thread_resume(WORLD_THREAD_SLOT_UPPER_MENU);
            g_world_set_ability_initialized = 0;
            world_menu_reset_selection_results();
            return 0;
        }
        g_world_set_ability_list_close_pending = 1;
    }
    lock_state = world_formation_get_unit_lock(g_unit_selected_index);
    if (lock_state == WORLD_FORMATION_UNIT_UNLOCKED) {
        lock_state = 0;
    }
    if (lock_state != 0) {
        world_menu_toggle_preview_stats_window(0);
        g_world_menu_description_text_id = 0;
        if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
            g_sound_queued_effect_id = MAIN_SFX_INVALID;
            return 1;
        }
        return 1;
    }
    if (g_world_set_ability_list_open == 0) {
        slot = g_world_set_ability_slot;
        current = g_world_unit_pointers[g_unit_selected_index]->abilities.slots[slot];
        g_world_formation_unit_cycle_mode = 2;
        g_world_menu_description_text_id = current;
        if (current == 0) {
            g_world_menu_description_text_id = -1;
        } else if (slot >= 2) {
            g_world_menu_description_text_id = current + 0x7800;
        } else {
            g_world_menu_description_text_id = current + 0x9800;
        }
        slot = menu_update_vertical_selection_and_mark_change(
                   4, WORLD_MENU_CURSOR_SET_ABILITY_SLOT, g_menu_input_secondary_repeat, MAIN_SFX_PAGE_SWITCH)
            + 1;
        g_world_set_ability_slot = slot;
        if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
            g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
            if (slot == 1) {
                g_world_set_ability_entry_count = ability_build_list_variant(g_unit_selected_index,
                    g_world_unit_pointers[g_unit_selected_index]->identity.job_id, MENU_ABILITY_CATEGORY_ACTION,
                    g_item_list_entries, slot);
            } else {
                g_world_set_ability_entry_count
                    = world_ability_build_equippable_rsm_list(g_unit_selected_index, slot - 1, g_item_list_entries);
            }
            menu_set_vsync_mode_and_event_speed(2);
            g_world_formation_unit_cycle_mode = 1;
            g_world_set_ability_list_open = 1;
            thread_suspend(WORLD_THREAD_SLOT_UPPER_MENU);
            g_world_ability_panel_thread_params.style = 1;
            g_world_set_ability_cursor = 0xFF;
            world_menu_get_selection_record(g_world_set_ability_slot + 4, &x, &y, g_item_list_entries);
            value = TEXT_SECTION_ABILITY_NAMES;
            if (g_world_set_ability_slot == 1) {
                value = TEXT_SECTION_SKILLSET_NAMES;
            }
            world_menu_init_scrollable_list(g_item_list_entries, x, y, g_text_section_pointers[value]);
        }
    } else {
        if (g_world_menu_option_count != 0) {
            cursor = g_world_menu_cursor_position;
            value = 0x7800;
            if (g_world_set_ability_slot == 1) {
                value = 0x9800;
            }
            g_world_menu_description_text_id = value + (g_item_list_entries[cursor] & ABILITY_LIST_ENTRY_ID_MASK);
            if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
                selected = g_item_list_entries[cursor];
                if (selected & ABILITY_LIST_ENTRY_DISABLED) {
                    world_text_show_message_and_play_sound(0xD81D, MAIN_SFX_ERROR_MESSAGE);
                } else {
                    value = world_ability_is_equip_support_change(g_unit_selected_index, selected);
                    world_formation_set_unit_ability_slot(g_unit_selected_index, g_world_set_ability_slot, selected,
                        value
                            | world_ability_is_equip_support_change(g_unit_selected_index,
                                g_world_unit_pointers[g_unit_selected_index]
                                    ->abilities.slots[g_world_set_ability_slot]));
                    g_sound_queued_effect_id = MAIN_SFX_EQUIP;
                    g_world_ability_menu_slots_changed = 1;
                    g_world_set_ability_list_close_pending = 1;
                    world_formation_stage_selected_unit();
                }
            }
        } else {
            g_world_menu_description_text_id = -1;
            if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
                if (g_world_set_ability_slot != 1) {
                    world_text_show_message_and_play_sound(0xC006, MAIN_SFX_ERROR_MESSAGE);
                } else {
                    g_sound_queued_effect_id = MAIN_SFX_INVALID;
                }
            }
        }
        if (g_world_set_ability_cursor != cursor) {
            g_world_set_ability_cursor = cursor;
            if ((g_item_list_entries[cursor] >= ABILITY_ID_MOVEMENT_MOVE_PLUS_1
                    && g_item_list_entries[cursor] <= ABILITY_ID_MOVEMENT_JUMP_PLUS_3)
                || (u16)((u16)g_world_unit_pointers[g_unit_selected_index]->abilities.slots[g_world_set_ability_slot]
                       - ABILITY_ID_MOVEMENT_MOVE_PLUS_1)
                    < 6) {
                world_ability_find_stat_changes_due_to_equipped(&g_world_item_preview_stat_detail,
                    g_world_unit_pointers[g_unit_selected_index]->abilities.slots[g_world_set_ability_slot],
                    g_item_list_entries[g_world_menu_cursor_position]);
                g_world_preview_stats_thread_params.redraw_request = 1;
                g_world_set_ability_show_stat_preview = 1;
            } else {
                g_world_set_ability_show_stat_preview = 0;
            }
        }
        menu_run_script_with_palette_mode(
            &g_world_set_ability_window_script, g_menu_input_primary_repeat, world_thread_is_task_active(0));
    }
    if (g_world_set_ability_list_close_pending != 0) {
        world_menu_set_window_scale_step(0);
        g_world_ability_panel_thread_params.style = 0;
        g_world_set_ability_list_open = 0;
        g_world_formation_unit_cycle_mode = 2;
        menu_set_vsync_mode_and_event_speed(0);
        g_world_set_ability_show_stat_preview = 0;
        g_menu_status_display_thread_params.redraw_request = 1;
        menu_set_selection_record(g_world_set_ability_slot + 4, g_world_menu_cursor_position,
            g_world_menu_scroll_offset, g_item_list_entries);
        thread_resume(WORLD_THREAD_SLOT_UPPER_MENU);
        g_world_set_ability_list_close_pending = 0;
    }
    busy = 0;
    world_menu_toggle_preview_stats_window(g_world_set_ability_show_stat_preview);
    g_world_set_ability_cursor_point.y = g_world_set_ability_slot * 16 + 0x90;
    world_menu_set_draw_priority(0x28);
    if (world_thread_is_task_active(0) != 0 || g_world_set_ability_list_open != 0) {
        busy = 1;
    }
    world_menu_draw_animated_cursor(&g_world_set_ability_cursor_point, &g_world_set_ability_cursor_anim, busy);
    return 1;
}
