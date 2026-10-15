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
    s16* items;
    s16* selected_item;
    s16* cursor_y;

    if (g_world_remove_item_unit_index != g_unit_selected_index) {
        g_world_item_best_remove_initialized = 0;
        g_world_remove_item_unit_index = g_unit_selected_index;
        g_menu_input_secondary_repeat = g_world_remove_item_saved_secondary_input;
        g_menu_input_primary_repeat = g_world_remove_item_saved_primary_input;
        g_menu_input_newly_pressed = g_world_remove_item_saved_newly_pressed;
        world_formation_update_unit_selection();
        menu_input_clear_state();
    }
    g_world_remove_item_saved_secondary_input = g_menu_input_secondary_repeat;
    g_world_remove_item_saved_primary_input = g_menu_input_primary_repeat;
    g_world_remove_item_saved_newly_pressed = g_menu_input_newly_pressed;
    if (g_world_item_best_remove_initialized == 0) {
        bcopy(g_world_unit_pointers[g_unit_selected_index]->equipment, g_world_remove_item_equipment,
            sizeof(g_world_remove_item_equipment));
        g_remove_item_previewed_slot = -1;
        g_world_formation_unit_cycle_mode = 2;
        g_remove_all_equipment_prompt_active = 0;
        g_world_item_best_remove_initialized = 1;
        g_world_remove_item_unit_index = g_unit_selected_index;
    }
    if (g_menu_input_primary_repeat & PSX_PAD_CROSS) {
        world_menu_toggle_preview_stats_window(0);
        g_world_item_best_remove_initialized = 0;
        g_sound_queued_effect_id = MAIN_SFX_CANCEL;
        return 0;
    }
    locked = world_formation_get_unit_lock(g_unit_selected_index);
    if (locked == WORLD_FORMATION_UNIT_UNLOCKED) {
        locked = 0;
    }
    if (locked != 0) {
        g_menu_help_message_text_id = 0;
        world_menu_toggle_preview_stats_window(0);
        if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
            g_sound_queued_effect_id = MAIN_SFX_INVALID;
        }
        return 1;
    }
    if (g_remove_all_equipment_prompt_active != 0) {
        g_remove_all_equipment_prompt_active = world_menu_handle_remove_all_equipment();
        if (g_remove_all_equipment_prompt_active == 0) {
            g_remove_item_previewed_slot = -2;
            bcopy(g_world_unit_pointers[g_unit_selected_index]->equipment, g_world_remove_item_equipment,
                sizeof(g_world_remove_item_equipment));
            return 1;
        }
        return 1;
    }
    if (g_menu_input_primary_repeat & PSX_PAD_LEFT) {
        g_remove_all_equipment_prompt_active = 1;
        item_calculate_equipment_swap_stat_delta(&g_item_preview_stat_detail, &g_menu_selected_unit_stat_summary,
            g_world_unit_pointers[g_unit_selected_index]->equipment, g_world_remove_all_equipment_ids);
        g_menu_status_display_thread_params.redraw_request = 1;
        g_menu_preview_stats_thread_params.redraw_request = 1;
        world_menu_toggle_preview_stats_window(1);
        g_sound_queued_effect_id = MAIN_SFX_PAGE_SWITCH;
    }
    slot = menu_update_vertical_selection_and_mark_change(
        WORLD_EQUIPMENT_SLOT_COUNT, WORLD_MENU_CURSOR_REMOVE_SLOT, g_menu_input_secondary_repeat, MAIN_SFX_PAGE_SWITCH);
    cursor_y = &g_world_remove_item_cursor_point.y;
    index = slot;
    *cursor_y = index * 16 + 0x90;
    gfx_set_otag_index(0x28);
    menu_update_and_draw_sliding_cursor_variant(
        &g_world_remove_item_cursor_point, &g_world_remove_item_cursor_anim, g_menu_input_lock_mode);
    items = g_world_remove_item_equipment;
    selected_item = &items[index];
    item = *selected_item;
    value = -1;
    if (item != ITEM_ID_NOTHING) {
        value = item + 0x6800;
    }
    g_menu_help_message_text_id = value;
    if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
        if (*selected_item == ITEM_ID_NOTHING) {
            g_sound_queued_effect_id = MAIN_SFX_INVALID;
        } else {
            world_formation_equip_item_to_unit_slot(g_unit_selected_index, slot, ITEM_ID_NOTHING);
            bcopy(
                g_world_unit_pointers[g_unit_selected_index]->equipment, items, sizeof(g_world_remove_item_equipment));
            world_formation_stage_selected_unit();
            g_sound_queued_effect_id = MAIN_SFX_UNEQUIP;
            g_remove_item_previewed_slot = -2;
        }
    }
    previous = g_remove_item_previewed_slot;
    if (previous != slot) {
        if (previous != -1) {
            g_menu_status_display_thread_params.redraw_request = 1;
        }
        g_remove_item_previewed_slot = slot;
        preview_item = (u8)g_world_remove_item_equipment[slot];
        if (preview_item != ITEM_ID_NOTHING) {
            world_formation_equip_item_to_unit_slot(WORLD_FORMATION_PREVIEW_UNIT_INDEX, slot, ITEM_ID_NOTHING);
            item_calculate_equipment_swap_stat_delta(&g_item_preview_stat_detail, &g_menu_selected_unit_stat_summary,
                g_world_unit_pointers[g_unit_selected_index]->equipment, g_world_formation_temp_unit->equipment);
            bcopy(g_world_unit_pointers[g_unit_selected_index]->equipment, g_world_formation_temp_unit->equipment,
                sizeof(g_world_formation_temp_unit->equipment));
            g_menu_preview_stats_thread_params.redraw_request = 1;
        }
        world_menu_toggle_preview_stats_window(preview_item != ITEM_ID_NOTHING);
    }
    return 1;
}
