#include "fft/event_equip.h"
#include "fft/menu.h"
#include "fft/menu_script.h"
#include "fft/thread.h"
#include "psx/libc.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Runs the slot list and, once a slot is confirmed, its item list.
 *
 * Confirming an item checks the inventory count, equips it and decrements
 * the count; moving the highlight equips the item on the scratch unit record
 * g_equip_unit_data[1] so the stat panel can preview the change. The WORLD
 * formation menu world_formation_run_equip_item_menu is the same routine plus an
 * equipment-lock check.
 *
 * The if/else spelling of the slot fallback is required: it keeps CSE from
 * reusing the stored slot for the category argument (the target reloads it)
 * and schedules the unit load ahead of the slot test. */
s32 equip_menu_run_equip_mode(void) {
    s16 selected;
    s16 scroll;
    s32 help_id;
    s32 item;
    s16 item_id;
    s32 slot;
    s32 result;
    s16 index;
    u16 entry;
    s8 selected_slot;
    s32 unit;

    if (g_equip_equip_mode_initialized == 0) {
        g_equip_equip_mode_initialized = 1;
        g_equip_equip_item_previewed_cursor = -1;
        g_equip_equip_item_list_open = 0;
        g_menu_list_selected_index = 0;
        g_equip_equip_item_preview_active = 0;
    }
    if (g_menu_input_primary_repeat & PSX_PAD_CROSS) {
        g_sound_queued_effect_id = MAIN_SFX_CANCEL;
        if (g_equip_equip_item_list_open == 0) {
            g_equip_equip_mode_initialized = 0;
            equip_menu_reset_selection_indices();
            return 0;
        }
        g_equip_equip_item_list_refresh = 1;
    }
    if (g_equip_equip_item_list_open == 0) {
        entry = g_equip_unit_data[g_equip_unit_selected_index]->equipment[g_equip_equip_item_slot];
        help_id = -1;
        if (entry != 0) {
            help_id = entry + 0x6800;
        }
        g_equip_text_help_message_id = help_id;
        selected_slot
            = menu_update_vertical_selection_and_mark_change(5, 0, g_menu_input_secondary_repeat, MAIN_SFX_PAGE_SWITCH);
        g_equip_equip_item_slot = selected_slot;
        if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
            unit = g_equip_unit_selected_index;
            if (selected_slot == 0) {
                slot = 1;
            } else {
                slot = selected_slot;
            }
            equip_item_build_filtered_list(
                unit, slot, g_equip_equip_item_slot, g_item_list_entries, ITEM_LIST_MODE_MARK_UNEQUIPPABLE);
            g_equip_equip_item_list_open = 1;
            g_equip_equipment_panel_style = 1;
            thread_suspend(EQUIP_THREAD_SLOT_COMMAND_MENU);
            equip_menu_restore_list_selection(g_equip_equip_item_slot != 0 ? g_equip_equip_item_slot - 1 : 0, &selected,
                &scroll, g_item_list_entries);
            menu_init_scrollable_list(
                g_item_list_entries, selected, scroll, g_text_section_pointers[TEXT_SECTION_ITEM_NAMES]);
            g_menu_row_callbacks[0].value = menu_get_entry_item_equipped_count;
            g_menu_row_callbacks[1].value = menu_get_entry_item_count;
            g_menu_row_callbacks[2].icon = menu_build_item_type_icon_source;
            g_menu_row_callbacks[3].icon = menu_get_entry_item_icon_source;
            g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
            g_equip_equip_item_previewed_cursor = -1;
        }
    } else {
        g_equip_text_help_message_id = g_menu_list_entry_count != 0
            ? (g_item_list_entries[g_menu_list_selected_index] & EQUIP_ITEM_LIST_ENTRY_ITEM_ID_MASK) + 0x6800
            : -1;
        item = g_menu_list_entry_count != 0
            ? g_item_list_entries[g_menu_list_selected_index] & EQUIP_ITEM_LIST_ENTRY_ITEM_ID_MASK
            : 0;
        if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
            if (g_menu_list_entry_count != 0
                && (g_item_list_entries[g_menu_list_selected_index] & EQUIP_ITEM_LIST_ENTRY_UNEQUIPPABLE) == 0) {
                item_id = item;
                if (item_adjust_inventory_count(item_id, 0) == 0) {
                    equip_text_show_message_with_sound(0xD809, MAIN_SFX_ERROR_MESSAGE);
                } else if (equip_unit_set_slot_item(g_equip_unit_selected_index, g_equip_equip_item_slot, item_id)
                    == EQUIP_SLOT_ITEM_ALLOWED) {
                    item_adjust_inventory_count(item_id, -1);
                    equip_unit_load_selected_data();
                    g_equip_equip_item_list_refresh = 1;
                    g_sound_queued_effect_id = MAIN_SFX_EQUIP;
                } else {
                    g_sound_queued_effect_id = MAIN_SFX_INVALID;
                }
            } else {
                g_sound_queued_effect_id = MAIN_SFX_INVALID;
            }
        }
        if (g_menu_list_entry_count != 0) {
            index = g_menu_list_selected_index;
            if (g_equip_equip_item_previewed_cursor != index) {
                g_equip_equip_item_previewed_cursor = index;
                result = equip_unit_set_slot_item(1, g_equip_equip_item_slot, item);
                if (result == EQUIP_SLOT_ITEM_ALLOWED) {
                    g_equip_equip_item_preview_active = 1;
                    g_equip_item_numeric_panel_redraw = result;
                    item_calculate_equipment_swap_stat_delta(&g_equip_item_preview_stat_detail,
                        &g_menu_selected_unit_stat_summary, g_equip_unit_data[g_equip_unit_selected_index]->equipment,
                        g_equip_unit_data[1]->equipment);
                    bcopy(
                        g_equip_unit_data[g_equip_unit_selected_index]->equipment, g_equip_unit_data[1]->equipment, 10);
                } else {
                    g_equip_equip_item_preview_active = 0;
                }
                g_equip_unit_status_panel_redraw = 1;
            }
        }
        menu_cmd_run_stream(&g_equip_equip_item_menu_script, g_menu_input_primary_repeat);
    }
    if (g_equip_equip_item_list_refresh != 0) {
        g_equip_equipment_panel_style = 0;
        g_equip_equip_item_list_open = 0;
        g_equip_equip_item_preview_active = 0;
        g_equip_unit_status_panel_redraw = 1;
        thread_resume(EQUIP_THREAD_SLOT_COMMAND_MENU);
        menu_set_selection_record(g_equip_equip_item_slot != 0 ? g_equip_equip_item_slot - 1 : 0,
            g_menu_list_selected_index, g_menu_scroll_base_index, g_item_list_entries);
        g_equip_equip_item_list_refresh = 0;
    }
    equip_panel_toggle_item_numeric_thread(g_equip_equip_item_preview_active);
    g_equip_slot_marker_rect.y = g_equip_equip_item_slot * 16 + 0x90;
    equip_gfx_set_otag_index(0x28);
    menu_update_and_draw_animated_cursor(&g_equip_slot_marker_rect, g_equip_equip_item_cursor_anim,
        g_equip_equip_item_list_open != 0 || g_menu_input_lock_mode != 0);
    return 1;
}
