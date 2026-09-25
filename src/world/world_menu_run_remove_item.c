#include "fft/data.h"
#include "fft/event.h"
#include "fft/main_sound.h"
#include "fft/world.h"
#include "psx/libc.h"
#include "psx/pad.h"

/* Handle single-slot removal and the remove-all confirmation.
 */
s32 world_menu_run_remove_item(void) {
    /* The target reserves 72 local bytes whose contents are not accessed. */
    s32 unused[18];
    u8 locked;
    u8 slot;
    s32 index;
    s32 value;
    s16 item;
    s8 previous;
    u8 preview_item;
    world_menu_halfword_view_t* items;
    world_menu_halfword_view_t* selected_item;
    s16* cursor_y;

    if (g_world_remove_item_unit_index != g_world_formation_selected_unit_index) {
        g_world_item_best_remove_initialized = 0;
        g_world_remove_item_unit_index = g_world_formation_selected_unit_index;
        g_world_input_secondary_repeat = g_world_remove_item_saved_secondary_input;
        g_world_input_primary_repeat = g_world_remove_item_saved_primary_input;
        g_world_input_newly_pressed = g_world_remove_item_saved_newly_pressed;
        world_formation_update_unit_selection();
        world_input_clear_state();
    }
    g_world_remove_item_saved_secondary_input = g_world_input_secondary_repeat;
    g_world_remove_item_saved_primary_input = g_world_input_primary_repeat;
    g_world_remove_item_saved_newly_pressed = g_world_input_newly_pressed;
    if (g_world_item_best_remove_initialized == 0) {
        bcopy(g_world_formation_unit_pointers[g_world_formation_selected_unit_index]->equipment,
            g_world_remove_item_equipment, 10);
        g_world_remove_item_previewed_slot = -1;
        g_world_formation_unit_cycle_mode = 2;
        g_world_remove_all_equipment_prompt_active = 0;
        g_world_item_best_remove_initialized = 1;
        g_world_remove_item_unit_index = g_world_formation_selected_unit_index;
    }
    if (g_world_input_primary_repeat & PSX_PAD_CROSS) {
        world_menu_toggle_preview_stats_window(0);
        g_world_item_best_remove_initialized = 0;
        g_world_menu_sound_effect_id = MAIN_SFX_CANCEL;
        return 0;
    }
    locked = world_formation_lock_equipment_and_abilities(g_world_formation_selected_unit_index);
    if (locked == 1) {
        locked = 0;
    }
    if (locked != 0) {
        g_world_menu_description_text_id = 0;
        world_menu_toggle_preview_stats_window(0);
        if (g_world_input_primary_repeat & PSX_PAD_CIRCLE) {
            g_world_menu_sound_effect_id = MAIN_SFX_INVALID;
        }
        return 1;
    }
    if (g_world_remove_all_equipment_prompt_active != 0) {
        g_world_remove_all_equipment_prompt_active = world_menu_handle_remove_all_equipment();
        if (g_world_remove_all_equipment_prompt_active == 0) {
            g_world_remove_item_previewed_slot = -2;
            bcopy(g_world_formation_unit_pointers[g_world_formation_selected_unit_index]->equipment,
                g_world_remove_item_equipment, 10);
            return 1;
        }
        return 1;
    }
    if (g_world_input_primary_repeat & PSX_PAD_LEFT) {
        g_world_remove_all_equipment_prompt_active = 1;
        world_item_calculate_equipment_swap_stat_delta(&g_world_item_preview_stat_detail,
            &g_world_selected_unit_stat_summary,
            g_world_formation_unit_pointers[g_world_formation_selected_unit_index]->equipment,
            g_world_remove_all_equipment_ids);
        g_world_formation_panel_windows[0].enabled = 1;
        g_world_preview_stats_thread_params.redraw_request = 1;
        world_menu_toggle_preview_stats_window(1);
        g_world_menu_sound_effect_id = MAIN_SFX_PAGE_SWITCH;
    }
    slot = world_menu_step_cursor_with_sound(5, 3, g_world_input_secondary_repeat, 6);
    cursor_y = &g_world_remove_item_cursor_point.y;
    index = slot;
    *cursor_y = index * 16 + 0x90;
    world_menu_set_draw_priority(0x28);
    world_menu_draw_animated_cursor(
        &g_world_remove_item_cursor_point, &g_world_remove_item_cursor_anim, g_world_thread_task_active);
    items = g_world_remove_item_equipment;
    selected_item = &items[index];
    item = selected_item->value;
    value = -1;
    if (item != ITEM_ID_NOTHING) {
        value = item + 0x6800;
    }
    g_world_menu_description_text_id = value;
    if (g_world_input_primary_repeat & PSX_PAD_CIRCLE) {
        if (selected_item->value == ITEM_ID_NOTHING) {
            g_world_menu_sound_effect_id = MAIN_SFX_INVALID;
        } else {
            world_formation_equip_item_to_unit_slot(g_world_formation_selected_unit_index, slot, ITEM_ID_NOTHING);
            bcopy(g_world_formation_unit_pointers[g_world_formation_selected_unit_index]->equipment, items, 10);
            world_formation_stage_selected_unit();
            g_world_menu_sound_effect_id = MAIN_SFX_UNEQUIP;
            g_world_remove_item_previewed_slot = -2;
        }
    }
    previous = g_world_remove_item_previewed_slot;
    if (previous != slot) {
        if (previous != -1) {
            g_world_formation_panel_windows[0].enabled = 1;
        }
        g_world_remove_item_previewed_slot = slot;
        preview_item = g_world_remove_item_equipment[slot].bytes[0];
        if (preview_item != ITEM_ID_NOTHING) {
            world_formation_equip_item_to_unit_slot(20, slot, ITEM_ID_NOTHING);
            world_item_calculate_equipment_swap_stat_delta(&g_world_item_preview_stat_detail,
                &g_world_selected_unit_stat_summary,
                g_world_formation_unit_pointers[g_world_formation_selected_unit_index]->equipment,
                g_world_formation_temp_unit->equipment);
            bcopy(g_world_formation_unit_pointers[g_world_formation_selected_unit_index]->equipment,
                g_world_formation_temp_unit->equipment, 10);
            g_world_preview_stats_thread_params.redraw_request = 1;
        }
        world_menu_toggle_preview_stats_window(preview_item != ITEM_ID_NOTHING);
    }
    return 1;
}
