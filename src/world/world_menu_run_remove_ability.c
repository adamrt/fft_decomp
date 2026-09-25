#include "fft/world.h"
#include "psx/libc.h"
#include "psx/pad.h"

/*
 * Run the ability-removal menu for the selected formation unit.
 *
 * Return 0 when canceled and 1 while active. The remove-all command opens its
 * confirmation; individual changes refresh the ability and stat previews.
 */
s32 world_menu_run_remove_ability(void) {
    s16 unit_index;
    u8 lock_state;
    u8 slot;
    u32 slot_index;
    u16 ability;
    s32 sound;
    s16* saved_slot;
    s16* saved_slots;
    s16* cursor_y;
    u16 movement_ability;
    s32 preview;

    unit_index = g_world_formation_selected_unit_index;
    if (g_world_remove_ability_unit_index != unit_index) {
        g_world_remove_ability_initialized = 0;
        g_world_remove_ability_unit_index
            = ((world_menu_halfword_view_t*)&g_world_formation_selected_unit_index)->bytes[0];
    }
    if (g_world_remove_ability_initialized == 0) {
        bcopy(((world_formation_unit_ability_slots_t*)g_world_formation_unit_pointers[unit_index])->ability_slots,
            g_world_remove_ability_slots, 10);
        g_world_remove_ability_cursor = -1;
        g_world_formation_unit_cycle_mode = 2;
        g_world_clear_abilities_prompt_active = 0;
        g_world_remove_ability_initialized = 1;
    }
    if (g_world_input_primary_repeat & PSX_PAD_CROSS) {
        world_menu_toggle_preview_stats_window(0);
        g_world_menu_sound_effect_id = MAIN_SFX_CANCEL;
        g_world_remove_ability_initialized = 0;
        g_world_remove_ability_unit_index = -1;
        return 0;
    }
    lock_state = world_formation_lock_equipment_and_abilities(g_world_formation_selected_unit_index);
    if (lock_state == 1) {
        lock_state = 0;
    }
    if (lock_state != 0) {
        g_world_menu_description_text_id = 0;
        world_menu_toggle_preview_stats_window(0);
        if (g_world_input_primary_repeat & PSX_PAD_CIRCLE) {
            g_world_menu_sound_effect_id = MAIN_SFX_INVALID;
        }
        return 1;
    }
    if (g_world_clear_abilities_prompt_active != 0) {
        g_world_clear_abilities_prompt_active = world_menu_update_clear_all_abilities_prompt();
        if (g_world_clear_abilities_prompt_active == 0) {
            g_world_remove_ability_cursor = -1;
            bcopy(((world_formation_unit_ability_slots_t*)
                          g_world_formation_unit_pointers[g_world_formation_selected_unit_index])
                      ->ability_slots,
                g_world_remove_ability_slots, 10);
            return 1;
        }
        return 1;
    }
    if (g_world_input_primary_repeat & PSX_PAD_LEFT) {
        world_ability_find_stat_changes_due_to_equipped(
            &g_world_item_preview_stat_detail, g_world_remove_ability_slots[4], 0);
        world_menu_toggle_preview_stats_window(1);
        g_world_clear_abilities_prompt_active = 1;
        g_world_preview_stats_thread_params.redraw_request = 1;
        g_world_menu_sound_effect_id = MAIN_SFX_PAGE_SWITCH;
    }
    slot = world_menu_step_cursor_with_sound(4, 5, g_world_input_secondary_repeat, 6) + 1;
    cursor_y = &g_world_remove_ability_cursor_point.y;
    slot_index = slot;
    *cursor_y = slot_index * 16 + 0x90;
    world_menu_set_draw_priority(0x28);
    world_menu_draw_animated_cursor(
        &g_world_remove_ability_cursor_point, &g_world_remove_ability_cursor_anim, g_world_thread_task_active);
    ability = ((world_formation_unit_ability_slots_t*)
                   g_world_formation_unit_pointers[g_world_formation_selected_unit_index])
                  ->ability_slots[slot_index];
    g_world_menu_description_text_id = ability;
    if (ability == 0) {
        g_world_menu_description_text_id = -1;
    } else if (slot_index >= 2) {
        g_world_menu_description_text_id = ability + 0x7800;
    } else {
        g_world_menu_description_text_id = ability + 0x9800;
    }
    if (g_world_input_primary_repeat & PSX_PAD_CIRCLE) {
        saved_slots = g_world_remove_ability_slots;
        saved_slot = saved_slots + slot;
        sound = *saved_slot;
        if (sound == 0) {
            sound = MAIN_SFX_INVALID;
        } else {
            world_formation_set_unit_ability_slot(g_world_formation_selected_unit_index, slot, 0,
                world_ability_is_equip_support_change(g_world_formation_selected_unit_index,
                    (s16)((world_formation_unit_ability_slots_t*)
                              g_world_formation_unit_pointers[g_world_formation_selected_unit_index])
                        ->ability_slots[slot]));
            g_world_ability_menu_slots_changed = 1;
            *saved_slot = 0;
            g_world_remove_ability_cursor = -1;
            world_formation_stage_selected_unit();
            sound = MAIN_SFX_UNEQUIP;
        }
        g_world_menu_sound_effect_id = sound;
    }
    if (g_world_remove_ability_cursor != slot) {
        g_world_remove_ability_cursor = slot;
        preview = 0;
        if (slot == 4) {
            movement_ability = g_world_remove_ability_slots[4];
            if ((u32)(movement_ability - ABILITY_ID_MOVEMENT_MOVE_PLUS_1) < 6) {
                world_ability_find_stat_changes_due_to_equipped(
                    &g_world_item_preview_stat_detail, (s16)movement_ability, 0);
                g_world_preview_stats_thread_params.redraw_request = 1;
                preview = 1;
            }
        }
        world_menu_toggle_preview_stats_window(preview);
    }
    return 1;
}
