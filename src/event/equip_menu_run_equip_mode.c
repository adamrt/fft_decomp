#include "fft/battle_text.h"
#include "fft/equip.h"
#include "fft/event.h"
#include "fft/main_sound.h"
#include "fft/thread.h"
#include "fft/world.h"
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
    u16 item_id;
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
        g_equip_menu_selected_list_index = 0;
        g_equip_equip_item_preview_active = 0;
    }
    if (g_equip_input_primary_repeat & PSX_PAD_CROSS) {
        g_equip_sound_queued_effect_id = MAIN_SFX_CANCEL;
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
        /* The target uses v0 from this void callee. */
        selected_slot = ((s32 (*)(s32, s32, s32, s8))equip_menu_update_vertical_selection_and_mark_change)(
            5, 0, g_equip_input_secondary_repeat, 6);
        g_equip_equip_item_slot = selected_slot;
        if (g_equip_input_primary_repeat & PSX_PAD_CIRCLE) {
            unit = g_equip_unit_selected_index;
            if (selected_slot == 0) {
                slot = 1;
            } else {
                slot = selected_slot;
            }
            /* The target passes sort_mode without the definition's u16 narrowing. */
            ((s32 (*)(s16, s32, s8, s16*, u8))equip_item_build_filtered_list)(
                unit, slot, g_equip_equip_item_slot, g_equip_item_list_entries, 1);
            g_equip_equip_item_list_open = 1;
            g_equip_equipment_panel_style = 1;
            battle_thread_suspend(5);
            equip_menu_restore_list_selection(g_equip_equip_item_slot != 0 ? g_equip_equip_item_slot - 1 : 0, &selected,
                &scroll, g_equip_item_list_entries);
            equip_menu_init_scrollable_list(
                g_equip_item_list_entries, selected, scroll, g_battle_text_section_pointers[7]);
            g_equip_menu_list_row_callbacks[0] = equip_item_get_available_with_equip_flag;
            g_equip_menu_list_row_callbacks[1] = equip_item_get_total_with_equip_flag;
            g_equip_menu_list_row_callbacks[2] = (equip_row_callback_t)equip_item_build_row_icon_rect;
            g_equip_menu_list_row_callbacks[3] = (equip_row_callback_t)equip_item_build_row_graphic_descriptor;
            g_equip_sound_queued_effect_id = MAIN_SFX_CONFIRM;
            g_equip_equip_item_previewed_cursor = -1;
        }
    } else {
        g_equip_text_help_message_id = g_equip_menu_list_entry_count != 0
            ? ((u16)g_equip_item_list_entries[g_equip_menu_selected_list_index] & 0x3FF) + 0x6800
            : -1;
        item = g_equip_menu_list_entry_count != 0
            ? (u16)g_equip_item_list_entries[g_equip_menu_selected_list_index] & 0x3FF
            : 0;
        if (g_equip_input_primary_repeat & PSX_PAD_CIRCLE) {
            if (g_equip_menu_list_entry_count != 0
                && ((u16)g_equip_item_list_entries[g_equip_menu_selected_list_index] & 0x4000) == 0) {
                item_id = item;
                if (equip_item_adjust_inventory_count(item_id, 0) == 0) {
                    equip_text_show_message_with_sound(0xD809, 0x30);
                } else if (equip_unit_set_slot_item(g_equip_unit_selected_index, g_equip_equip_item_slot, item_id)
                    == 1) {
                    equip_item_adjust_inventory_count(item_id, -1);
                    equip_unit_load_selected_data();
                    g_equip_equip_item_list_refresh = 1;
                    g_equip_sound_queued_effect_id = MAIN_SFX_EQUIP;
                } else {
                    g_equip_sound_queued_effect_id = MAIN_SFX_INVALID;
                }
            } else {
                g_equip_sound_queued_effect_id = MAIN_SFX_INVALID;
            }
        }
        if (g_equip_menu_list_entry_count != 0) {
            index = g_equip_menu_selected_list_index;
            if (g_equip_equip_item_previewed_cursor != index) {
                g_equip_equip_item_previewed_cursor = index;
                result = equip_unit_set_slot_item(1, g_equip_equip_item_slot, item);
                if (result == 1) {
                    g_equip_equip_item_preview_active = 1;
                    g_equip_item_numeric_panel_redraw = result;
                    equip_unit_calculate_equipment_swap_stat_deltas(&g_equip_item_preview_stat_detail,
                        &g_equip_selected_unit_stat_summary, g_equip_unit_data[g_equip_unit_selected_index]->equipment,
                        g_equip_unit_data[1]->equipment);
                    bcopy(
                        g_equip_unit_data[g_equip_unit_selected_index]->equipment, g_equip_unit_data[1]->equipment, 10);
                } else {
                    g_equip_equip_item_preview_active = 0;
                }
                g_equip_unit_status_panel_redraw = 1;
            }
        }
        equip_cmd_run_stream(g_equip_equip_item_menu_script, g_equip_input_primary_repeat);
    }
    if (g_equip_equip_item_list_refresh != 0) {
        g_equip_equipment_panel_style = 0;
        g_equip_equip_item_list_open = 0;
        g_equip_equip_item_preview_active = 0;
        g_equip_unit_status_panel_redraw = 1;
        battle_thread_resume(5);
        equip_menu_set_selection_record(g_equip_equip_item_slot != 0 ? g_equip_equip_item_slot - 1 : 0,
            g_equip_menu_selected_list_index, g_equip_menu_scroll_base_index, (u16*)g_equip_item_list_entries);
        g_equip_equip_item_list_refresh = 0;
    }
    equip_panel_toggle_item_numeric_thread(g_equip_equip_item_preview_active);
    g_equip_slot_marker_rect.y = g_equip_equip_item_slot * 16 + 0x90;
    equip_gfx_set_otag_index(0x28);
    equip_menu_update_and_draw_animated_marker(&g_equip_slot_marker_rect, g_equip_equip_item_cursor_anim,
        g_equip_equip_item_list_open != 0 || g_event_mode != 0);
    return 1;
}
