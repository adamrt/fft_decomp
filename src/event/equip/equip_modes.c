/* Equip screen state, the equip and remove modes, the clear-all prompt and the preview HP bonus. */

#include "fft/event_equip.h"
#include "fft/menu.h"
#include "fft/menu_script.h"
#include "fft/thread.h"
#include "psx/libc.h"
#include "psx/pad.h"
#include "psx/types.h"

void equip_menu_update_screen_state(void) {
    s32 result;
    s32 state;
    s32 mode;
    s16* value_ptr;

    if (g_equip_menu_thread_running == 0) {
        if (thread_is_running(EQUIP_THREAD_SLOT_COMMAND_MENU) == 0) {
            g_equip_command_menu.selected_index = 0;
            g_menu_thread_menu_data = &g_equip_command_menu;
            thread_start(EQUIP_THREAD_SLOT_COMMAND_MENU, menu_icon_list_thread);
            thread_set_parameters(EQUIP_THREAD_SLOT_COMMAND_MENU, (s32)g_menu_thread_menu_data, 0, 0);
            g_equip_thread_idle_stop_pending = 0;
            g_equip_menu_thread_running = 1;
        }
    }
    if (g_equip_thread_idle_stop_pending != 0) {
        thread_suspend(EQUIP_THREAD_SLOT_COMMAND_MENU_CHILD);
        g_equip_thread_idle_stop_pending = 0;
    }
    state = g_equip_menu_screen_state;
    if (state == EQUIP_SCREEN_STATE_COMMAND_MENU) {
        value_ptr = &g_equip_menu_result_rows[0];
        mode = *value_ptr;
        g_menu_help_message_text_id = g_equip_command_menu.selected_index + 0x1029;
        if (mode == 1) {
            g_sound_queued_effect_id = MAIN_SFX_INVALID;
            equip_menu_reset_selection_indices();
        } else if (mode != -1) {
            thread_start(EQUIP_THREAD_SLOT_COMMAND_MENU_CHILD, thread_wait_forever_by_frames);
            g_equip_menu_screen_state = *value_ptr + EQUIP_SCREEN_STATE_EQUIP;
            menu_set_selection_value(0, 0);
            menu_set_selection_value(1, 0);
            menu_set_selection_value(2, 0);
            equip_menu_reset_selection_indices();
        }
        g_equip_menu_thread_running = thread_is_running(EQUIP_THREAD_SLOT_COMMAND_MENU);
        if (g_equip_menu_thread_running == 0) {
            g_equip_menu_screen_state = EQUIP_SCREEN_STATE_CLOSED;
        }
        return;
    }
    switch (state) {
    case EQUIP_SCREEN_STATE_EQUIP:
        result = equip_menu_run_equip_mode();
        break;
    case EQUIP_SCREEN_STATE_REMOVE:
        result = equip_menu_run_remove_mode();
        break;
    case EQUIP_SCREEN_STATE_ITEM_LIST:
        result = menu_update_equipment_variant();
        break;
    }
    if (result == 0) {
        g_equip_menu_screen_state = EQUIP_SCREEN_STATE_COMMAND_MENU;
        g_equip_thread_idle_stop_pending = 1;
    }
}

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
        g_equip_item_previewed_cursor = -1;
        g_equip_item_list_open = 0;
        g_menu_list_selected_index = 0;
        g_equip_item_preview_active = 0;
    }
    if (g_menu_input_primary_repeat & PSX_PAD_CROSS) {
        g_sound_queued_effect_id = MAIN_SFX_CANCEL;
        if (g_equip_item_list_open == 0) {
            g_equip_equip_mode_initialized = 0;
            equip_menu_reset_selection_indices();
            return 0;
        }
        g_equip_item_list_refresh = 1;
    }
    if (g_equip_item_list_open == 0) {
        entry = g_equip_unit_data[g_equip_unit_selected_index]->equipment[g_equip_item_slot];
        help_id = -1;
        if (entry != 0) {
            help_id = entry + 0x6800;
        }
        g_menu_help_message_text_id = help_id;
        selected_slot
            = menu_update_vertical_selection_and_mark_change(5, 0, g_menu_input_secondary_repeat, MAIN_SFX_PAGE_SWITCH);
        g_equip_item_slot = selected_slot;
        if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
            unit = g_equip_unit_selected_index;
            if (selected_slot == 0) {
                slot = 1;
            } else {
                slot = selected_slot;
            }
            equip_item_build_filtered_list(
                unit, slot, g_equip_item_slot, g_item_list_entries, ITEM_LIST_MODE_MARK_UNEQUIPPABLE);
            g_equip_item_list_open = 1;
            g_equip_equipment_panel_style = 1;
            thread_suspend(EQUIP_THREAD_SLOT_COMMAND_MENU);
            equip_menu_restore_list_selection(
                g_equip_item_slot != 0 ? g_equip_item_slot - 1 : 0, &selected, &scroll, g_item_list_entries);
            menu_init_scrollable_list(
                g_item_list_entries, selected, scroll, g_text_section_pointers[TEXT_SECTION_ITEM_NAMES]);
            g_menu_row_callbacks[0].value = menu_get_entry_item_equipped_count;
            g_menu_row_callbacks[1].value = menu_get_entry_item_count;
            g_menu_row_callbacks[2].icon = menu_build_item_type_icon_source;
            g_menu_row_callbacks[3].icon = menu_get_entry_item_icon_source;
            g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
            g_equip_item_previewed_cursor = -1;
        }
    } else {
        g_menu_help_message_text_id = g_menu_list_entry_count != 0
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
                } else if (equip_unit_set_slot_item(g_equip_unit_selected_index, g_equip_item_slot, item_id)
                    == EQUIP_SLOT_ITEM_ALLOWED) {
                    item_adjust_inventory_count(item_id, -1);
                    equip_unit_load_selected_data();
                    g_equip_item_list_refresh = 1;
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
            if (g_equip_item_previewed_cursor != index) {
                g_equip_item_previewed_cursor = index;
                result = equip_unit_set_slot_item(1, g_equip_item_slot, item);
                if (result == EQUIP_SLOT_ITEM_ALLOWED) {
                    g_equip_item_preview_active = 1;
                    g_menu_preview_stats_thread_params.redraw_request = result;
                    item_calculate_equipment_swap_stat_delta(&g_item_preview_stat_detail,
                        &g_menu_selected_unit_stat_summary, g_equip_unit_data[g_equip_unit_selected_index]->equipment,
                        g_equip_unit_data[1]->equipment);
                    bcopy(
                        g_equip_unit_data[g_equip_unit_selected_index]->equipment, g_equip_unit_data[1]->equipment, 10);
                } else {
                    g_equip_item_preview_active = 0;
                }
                g_equip_unit_status_panel_redraw = 1;
            }
        }
        menu_cmd_run_stream_variant(&g_equip_item_menu_script, g_menu_input_primary_repeat);
    }
    if (g_equip_item_list_refresh != 0) {
        g_equip_equipment_panel_style = 0;
        g_equip_item_list_open = 0;
        g_equip_item_preview_active = 0;
        g_equip_unit_status_panel_redraw = 1;
        thread_resume(EQUIP_THREAD_SLOT_COMMAND_MENU);
        menu_set_selection_record(g_equip_item_slot != 0 ? g_equip_item_slot - 1 : 0, g_menu_list_selected_index,
            g_menu_scroll_base_index, g_item_list_entries);
        g_equip_item_list_refresh = 0;
    }
    equip_panel_toggle_item_numeric_thread(g_equip_item_preview_active);
    g_equip_slot_marker_rect.y = g_equip_item_slot * 16 + 0x90;
    gfx_set_otag_index(0x28);
    menu_update_and_draw_animated_cursor(&g_equip_slot_marker_rect, g_equip_equip_item_cursor_anim,
        g_equip_item_list_open != 0 || g_menu_input_lock_mode != 0);
    return 1;
}

s32 equip_menu_run_remove_mode(void) {
    /* Never referenced; the target frame reserves these bytes. */
    u8 unused[0x48];
    s32 i;
    u32 input;
    s32 selection;
    u8 has_item;

    if (g_equip_remove_mode_initialized == 0) {
        for (i = 0; i < 5; i++) {
            g_remove_item_equipment[i] = g_equip_unit_data[g_equip_unit_selected_index]->equipment[i];
        }
        g_remove_item_previewed_slot = -1;
        g_remove_all_equipment_prompt_active = 0;
        g_equip_remove_mode_initialized = 1;
    }
    input = g_menu_input_primary_repeat;
    if (input & PSX_PAD_CROSS) {
        equip_panel_toggle_item_numeric_thread(0);
        g_equip_unit_status_panel_redraw = 1;
        g_equip_remove_mode_initialized = 0;
        g_sound_queued_effect_id = MAIN_SFX_CANCEL;
        return 0;
    }
    if (g_remove_all_equipment_prompt_active != 0) {
        g_remove_all_equipment_prompt_active = equip_menu_update_clear_all_equipment_prompt();
        if (g_remove_all_equipment_prompt_active == 0) {
            g_remove_item_previewed_slot = -2;
            bcopy(g_equip_unit_data[g_equip_unit_selected_index]->equipment, g_remove_item_equipment, 10);
        }
        return 1;
    }
    if (input & PSX_PAD_LEFT) {
        g_remove_all_equipment_prompt_active = 1;
        item_calculate_equipment_swap_stat_delta(&g_item_preview_stat_detail, &g_menu_selected_unit_stat_summary,
            g_equip_unit_data[g_equip_unit_selected_index]->equipment, g_remove_all_equipment_ids);
        g_equip_unit_status_panel_redraw = 1;
        g_menu_preview_stats_thread_params.redraw_request = 1;
        equip_panel_toggle_item_numeric_thread(1);
        g_sound_queued_effect_id = MAIN_SFX_PAGE_SWITCH;
    }
    selection
        = menu_update_vertical_selection_and_mark_change(5, 1, g_menu_input_secondary_repeat, MAIN_SFX_PAGE_SWITCH);
    g_equip_remove_item_marker_rect.y = ((selection & 0xFF) << 4) + 0x90;
    gfx_set_otag_index(0x28);
    menu_update_and_draw_animated_cursor(
        &g_equip_remove_item_marker_rect, g_equip_remove_item_cursor_anim, g_menu_input_lock_mode);

    g_menu_help_message_text_id
        = g_remove_item_equipment[selection & 0xFF] != 0 ? g_remove_item_equipment[selection & 0xFF] + 0x6800 : -1;
    if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
        if (g_remove_item_equipment[selection & 0xFF] == 0
            || equip_unit_set_slot_item(g_equip_unit_selected_index, (u8)selection, 0)
                == EQUIP_SLOT_ITEM_CURRENT_LOCKED) {
            g_sound_queued_effect_id = MAIN_SFX_INVALID;
        } else {
            bcopy(g_equip_unit_data[g_equip_unit_selected_index]->equipment, g_remove_item_equipment, 10);
            equip_unit_load_selected_data();
            g_sound_queued_effect_id = MAIN_SFX_UNEQUIP;
            g_remove_item_previewed_slot = -1;
        }
    }
    if (g_remove_item_previewed_slot != (selection & 0xFF)) {
        g_remove_item_previewed_slot = selection;
        has_item = g_remove_item_equipment[selection & 0xFF];
        if (has_item != 0) {
            equip_unit_set_slot_item(1, (u8)selection, 0);
            item_calculate_equipment_swap_stat_delta(&g_item_preview_stat_detail, &g_menu_selected_unit_stat_summary,
                g_equip_unit_data[g_equip_unit_selected_index]->equipment, g_equip_unit_data[1]->equipment);
            bcopy(g_equip_unit_data[g_equip_unit_selected_index]->equipment, g_equip_unit_data[1]->equipment, 10);
            g_menu_preview_stats_thread_params.redraw_request = 1;
        }
        g_equip_unit_status_panel_redraw = 1;
        equip_panel_toggle_item_numeric_thread(has_item != 0);
    }
    return 1;
}

/*
 * Update the clear-all-equipment prompt and empty every occupied slot when accepted.
 *
 * Returns 1 while waiting for input and 0 after the prompt is accepted or dismissed.
 */
s32 equip_menu_update_clear_all_equipment_prompt(void) {
    u8 unused_buffer[0x40]; /* unreferenced; reserves the target's frame */
    u16 marker_state;
    s32 index;
    s32 unequipped;
    s32 flags;

    g_menu_help_message_text_id = -1;
    gfx_set_otag_index(0x28);
    g_equip_menu_clear_all_prompt_frame = g_equip_menu_clear_all_prompt_frame + 1;
    if (g_equip_menu_clear_all_prompt_frame & 1) {
        for (index = 0; index < 5; index++) {
            g_equip_menu_clear_all_prompt_draw_params.y = 0x90 + index * 0x10;
            marker_state = 0;
            menu_update_and_draw_animated_cursor(
                &g_equip_menu_clear_all_prompt_draw_params, &marker_state, g_menu_input_lock_mode);
        }
    }
    flags = g_menu_input_primary_repeat;
    if (flags & PSX_PAD_RIGHT) {
        g_sound_queued_effect_id = MAIN_SFX_PAGE_SWITCH;
        return 0;
    }
    if (flags & PSX_PAD_CIRCLE) {
        unequipped = 0;
        for (index = 0; index < 5; index++) {
            if (g_equip_unit_data[g_equip_unit_selected_index]->equipment[index] != 0) {
                unequipped = 1;
                equip_unit_set_slot_item(g_equip_unit_selected_index, index, 0);
            }
        }
        equip_unit_load_selected_data();
        if (unequipped == 0) {
            g_sound_queued_effect_id = MAIN_SFX_INVALID;
        } else {
            g_sound_queued_effect_id = MAIN_SFX_UNEQUIP;
        }
        return 0;
    }
    return 1;
}

s32 equip_menu_get_preview_hp_bonus(s32 unused_row) {
    return g_menu_selected_unit_stat_summary.gauges[MENU_UNIT_STATUS_GAUGE_HP].bonus;
}

s32 equip_menu_get_preview_mp_bonus(s32 unused_row) {
    return g_menu_selected_unit_stat_summary.gauges[MENU_UNIT_STATUS_GAUGE_MP].bonus;
}
