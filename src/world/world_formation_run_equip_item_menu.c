#include "fft/thread.h"
#include "fft/world.h"
#include "psx/libc.h"
#include "psx/pad.h"

/* Run the formation "Equip" item menu: step the slot cursor, open the item
 * list for the selected slot, equip the highlighted item and keep the stat
 * preview in sync. Returns 0 when the menu is canceled, 1 otherwise.
 */
s32 world_formation_run_equip_item_menu(void) {
    s16 x;
    s16 y;
    u8 locked;
    s32 slot;
    s32 item;
    /* s16: the conversion from item keeps CSE from folding the copy away
       (the target moves item into a second saved register here). */
    s16 equip;
    u16 equipped;
    s32 value;
    s16* cursor_y;

    /* The target tests the init flag with lb; the header types it u8. */
    if (g_world_equip_menu_initialized == 0) {
        g_world_equip_menu_initialized = 1;
        g_world_equip_item_previewed_cursor = -1;
        g_world_equip_item_list_open = 0;
        g_world_formation_unit_cycle_mode = 2;
        g_world_equip_item_slot = 0;
        g_world_equip_item_list_refresh = 0;
        g_world_equip_item_preview_active = 0;
    }
    if (g_menu_input_primary_repeat & PSX_PAD_CROSS) {
        g_sound_queued_effect_id = MAIN_SFX_CANCEL;
        if (g_world_equip_item_list_open == 0) {
            g_world_equip_menu_initialized = 0;
            world_menu_reset_selection_results();
            return 0;
        }
        g_world_equip_item_list_refresh = 1;
    }
    locked = world_formation_get_unit_lock(g_unit_selected_index);
    if (locked == WORLD_FORMATION_UNIT_UNLOCKED) {
        locked = 0;
    }
    if (locked != 0) {
        world_menu_toggle_preview_stats_window(0);
        g_menu_help_message_text_id = 0;
        if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
            g_sound_queued_effect_id = MAIN_SFX_INVALID;
        }
        return 1;
    }
    if (g_world_equip_item_list_open == 0) {
        g_world_equip_item_slot = menu_update_vertical_selection_and_mark_change(WORLD_EQUIPMENT_SLOT_COUNT,
            WORLD_MENU_CURSOR_EQUIP_SLOT, g_menu_input_secondary_repeat, MAIN_SFX_PAGE_SWITCH);
        if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
            world_item_build_category_list(g_unit_selected_index, g_world_equip_item_slot + 1, g_world_equip_item_slot,
                g_item_list_entries, ITEM_LIST_MODE_MARK_UNEQUIPPABLE);
            menu_set_vsync_mode_and_event_speed(2);
            g_world_formation_unit_cycle_mode = 1;
            g_world_equip_item_list_open = 1;
            g_world_change_banner_panel_thread_params.style = 1;
            thread_suspend(WORLD_THREAD_SLOT_UPPER_MENU);
            slot = 0;
            if (g_world_equip_item_slot != 0) {
                slot = g_world_equip_item_slot - 1;
            }
            menu_get_selection_record_variant(slot, &x, &y, g_item_list_entries);
            menu_init_scrollable_list(g_item_list_entries, x, y, g_text_section_pointers[TEXT_SECTION_ITEM_NAMES]);
            g_world_menu_script_callbacks[0].value = menu_get_entry_item_equipped_count;
            g_world_menu_script_callbacks[1].value = menu_get_entry_item_count;
            g_world_menu_script_callbacks[2].icon = menu_build_item_type_icon_source;
            g_world_menu_script_callbacks[3].icon = menu_get_entry_item_icon_source;
            g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
            g_world_equip_item_previewed_cursor = -1;
        }
        equipped = g_world_unit_pointers[g_unit_selected_index]->equipment[g_world_equip_item_slot];
        value = -1;
        if (equipped != ITEM_ID_NOTHING) {
            value = equipped + 0x6800;
        }
        g_menu_help_message_text_id = value;
    } else {
        g_menu_help_message_text_id = g_menu_list_entry_count != 0
            ? (g_item_list_entries[g_menu_list_selected_index] & WORLD_ITEM_ID_MASK) + 0x6800
            : -1;
        item = g_menu_list_entry_count != 0 ? g_item_list_entries[g_menu_list_selected_index] & WORLD_ITEM_ID_MASK : 0;
        if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
            if (g_menu_list_entry_count == 0
                || (g_item_list_entries[g_menu_list_selected_index] & WORLD_ITEM_LIST_ENTRY_UNEQUIPPABLE)) {
                g_sound_queued_effect_id = MAIN_SFX_INVALID;
            } else {
                equip = item;
                if (item_adjust_inventory_count(equip, 0) == 0) {
                    world_text_show_message_and_play_sound(0xc003, MAIN_SFX_ERROR_MESSAGE);
                } else if (world_formation_equip_item_to_unit_slot(
                               g_unit_selected_index, g_world_equip_item_slot, equip)
                    == 1) {
                    item_adjust_inventory_count(equip, -1);
                    world_formation_stage_selected_unit();
                    g_world_equip_item_list_refresh = 1;
                    g_sound_queued_effect_id = MAIN_SFX_EQUIP;
                } else {
                    g_sound_queued_effect_id = MAIN_SFX_INVALID;
                }
            }
        }
        if (g_menu_list_entry_count != 0 && g_world_equip_item_previewed_cursor != g_menu_list_selected_index) {
            g_world_equip_item_previewed_cursor = g_menu_list_selected_index;
            if (world_formation_equip_item_to_unit_slot(
                    WORLD_FORMATION_PREVIEW_UNIT_INDEX, g_world_equip_item_slot, item)
                == 1) {
                g_world_equip_item_preview_active = 1;
                g_menu_preview_stats_thread_params.redraw_request = 1;
                item_calculate_equipment_swap_stat_delta(&g_item_preview_stat_detail,
                    &g_menu_selected_unit_stat_summary, g_world_unit_pointers[g_unit_selected_index]->equipment,
                    g_world_formation_temp_unit->equipment);
                bcopy(g_world_unit_pointers[g_unit_selected_index]->equipment, g_world_formation_temp_unit->equipment,
                    10);
            } else {
                g_world_equip_item_preview_active = 0;
            }
            g_menu_status_display_thread_params.redraw_request = 1;
        }
        menu_cmd_run_stream_variant(&g_world_equip_item_menu_script, g_menu_input_primary_repeat);
    }
    if (g_world_equip_item_list_refresh != 0) {
        g_world_change_banner_panel_thread_params.style = 0;
        g_world_equip_item_list_open = 0;
        g_world_formation_unit_cycle_mode = 2;
        menu_set_vsync_mode_and_event_speed(0);
        g_world_equip_item_preview_active = 0;
        g_menu_status_display_thread_params.redraw_request = 1;
        thread_resume(WORLD_THREAD_SLOT_UPPER_MENU);
        slot = 0;
        if (g_world_equip_item_slot != 0) {
            slot = g_world_equip_item_slot - 1;
        }
        menu_set_selection_record(slot, g_menu_list_selected_index, g_menu_scroll_base_index, g_item_list_entries);
        g_world_equip_item_list_refresh = 0;
    }
    world_menu_toggle_preview_stats_window(g_world_equip_item_preview_active);
    cursor_y = &g_world_equip_item_cursor_point.y;
    *cursor_y = g_world_equip_item_slot * 16 + 0x90;
    world_menu_set_draw_priority(0x28);
    /* The mode is an argument expression so &g_world_equip_item_cursor_point is formed before its
       branches and derived from cursor_y (addiu a0,s0,-2), as in the target. */
    world_menu_draw_animated_cursor(&g_world_equip_item_cursor_point, &g_world_equip_item_cursor_anim,
        g_world_equip_item_list_open != 0 || g_menu_input_lock_mode != 0);
    return 1;
}
