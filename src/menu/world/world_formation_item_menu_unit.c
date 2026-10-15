#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/world.h"
#include "psx/libc.h"
#include "psx/pad.h"
#include "psx/types.h"

/*
 * Dispatch formation item-menu transitions and its submenus (6:
 * world_formation_run_equip_item_menu, 7: best-fit equipment, 8: remove item, 9:
 * world_formation_run_item_submenu_step).
 *
 * States 0x10/0x17 slide the grid in, 0x11 slides it back out; state 1 runs
 * the icon list menu (g_world_item_icon_menu) and enters submenu 6..9 from the selected
 * row, refreshing the ordered unit list by roster slot when the list closes.
 * Units world_formation_get_unit_lock reports as locked get text 0xD802.
 */
void world_formation_item_menu(void) {
    s32 state;
    s8 running;
    u8 saved;
    s32 done;
    s32 roster_slot;
    s16* selection;

    state = g_world_formation_current_menu;
    if (state == WORLD_FORMATION_MENU_ITEM_ENTER || state == WORLD_FORMATION_MENU_ITEM_ENTER_NO_SLIDE) {
        if (g_world_item_menu_initialized == 0) {
            saved = g_world_formation_unit_browse_enabled;
            g_world_formation_unit_banner_at_bottom = 0;
            g_world_formation_unit_browse_enabled = 0;
            g_world_item_menu_saved_browse_enabled = saved;
            menu_request_thread_stop(THREAD_SCREEN_SLOT_CHARACTER_STATUS);
            g_world_item_menu_initialized = 1;
        }
        if (g_world_formation_current_menu == WORLD_FORMATION_MENU_ITEM_ENTER) {
            g_world_item_menu_initialized = world_formation_step_grid_transition(0);
        } else {
            g_world_item_menu_initialized = 0;
        }
        if (g_world_item_menu_initialized == 0) {
            g_world_formation_current_menu = WORLD_FORMATION_MENU_ITEM;
        }
        return;
    }
    if (state == WORLD_FORMATION_MENU_ITEM_LEAVE) {
        running = world_formation_step_grid_return_transition(1);
        g_world_item_menu_initialized = running;
        if (running == 0) {
            g_world_formation_current_menu = WORLD_FORMATION_MENU_UNIT_GRID;
            g_world_formation_triangle_menu.selected_index = 0;
        }
        return;
    }
    if (g_world_item_menu_initialized == 0 && thread_is_running(WORLD_THREAD_SLOT_UPPER_MENU) == 0) {
        g_world_item_icon_menu.selected_index = 0;
        g_menu_thread_menu_data = &g_world_item_icon_menu;
        thread_start(WORLD_THREAD_SLOT_UPPER_MENU, menu_icon_list_thread);
        thread_set_parameters(WORLD_THREAD_SLOT_UPPER_MENU, (s32)g_menu_thread_menu_data, 0, 0);
        g_world_item_menu_unit_index = 0xFF;
        menu_clear_selection_records();
        g_world_formation_unit_banner_enabled = 1;
        g_world_item_submenu_exit_pending = 0;
        g_world_item_menu_initialized = 1;
    }
    if (g_world_formation_current_menu < 9) {
        world_formation_place_unit_portrait_at_rest(g_unit_selected_index);
    }
    if (g_world_item_menu_unit_index != g_unit_selected_index) {
        g_world_item_menu_unit_index = (u8)g_unit_selected_index;
        if (world_formation_get_unit_lock(g_unit_selected_index) < 0) {
            g_world_item_icon_menu.text_id = 0xD802;
        } else {
            g_world_item_icon_menu.text_id = 0xD801;
        }
        g_world_menu_text_redraw_request = 1;
    }
    if (g_world_item_submenu_exit_pending != 0) {
        thread_suspend(WORLD_THREAD_SLOT_UPPER_MENU_CHILD);
        g_world_item_submenu_exit_pending = 0;
    }
    if (g_world_formation_current_menu == WORLD_FORMATION_MENU_ITEM) {
        g_world_formation_unit_cycle_mode = 2;
        if (g_world_menu_selection_results[1] >= 0 && g_world_menu_selection_results[1] < 3
            && world_formation_get_unit_lock(g_unit_selected_index) < 0) {
            g_sound_queued_effect_id = MAIN_SFX_INVALID;
            world_menu_reset_selection_results();
            return;
        }
        selection = &g_world_menu_selection_results[1];
        if (*selection != -1) {
            g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
            thread_start(WORLD_THREAD_SLOT_UPPER_MENU_CHILD, thread_wait_forever_by_frames);
            g_world_formation_current_menu = *selection + WORLD_FORMATION_MENU_ITEM_EQUIP;
            menu_init_selection_value(WORLD_MENU_CURSOR_EQUIP_SLOT, 0);
            menu_init_selection_value(WORLD_MENU_CURSOR_REMOVE_SLOT, 0);
            menu_init_selection_value(WORLD_MENU_CURSOR_ITEM_TAB, 0);
        }
        running = thread_is_running(WORLD_THREAD_SLOT_UPPER_MENU);
        g_world_item_menu_initialized = running;
        if (running == 0) {
            menu_toggle_change_banner_panel_thread(0);
            roster_slot = g_world_unit_pointers[g_unit_selected_index]->identity.roster_slot;
            world_formation_build_record_list(0, g_world_unit_pointers, WORLD_FORMATION_RECORD_LIST_ALL);
            g_unit_selected_index = world_formation_find_index_by_roster_slot(roster_slot);
            g_world_formation_current_menu = WORLD_FORMATION_MENU_ITEM_LEAVE;
            g_world_formation_unit_browse_enabled = g_world_item_menu_saved_browse_enabled;
            g_world_formation_unit_banner_enabled = g_world_formation_saved_banner_enabled;
        }
        return;
    }
    switch (g_world_formation_current_menu) {
    case WORLD_FORMATION_MENU_ITEM_EQUIP:
        done = world_formation_run_equip_item_menu();
        break;
    case WORLD_FORMATION_MENU_ITEM_BEST_FIT:
        done = world_menu_run_best_fit_equipment();
        break;
    case WORLD_FORMATION_MENU_ITEM_REMOVE:
        done = world_menu_run_remove_item();
        break;
    case WORLD_FORMATION_MENU_ITEM_LIST:
        done = menu_update_equipment_variant();
        break;
    }
    if (done == 0) {
        g_world_item_menu_unit_index = 0xFF;
        world_formation_stage_selected_unit();
        g_world_formation_current_menu = WORLD_FORMATION_MENU_ITEM;
        g_world_item_submenu_exit_pending = 1;
    }
}

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
        g_equip_item_previewed_cursor = -1;
        g_equip_item_list_open = 0;
        g_world_formation_unit_cycle_mode = 2;
        g_equip_item_slot = 0;
        g_equip_item_list_refresh = 0;
        g_equip_item_preview_active = 0;
    }
    if (g_menu_input_primary_repeat & PSX_PAD_CROSS) {
        g_sound_queued_effect_id = MAIN_SFX_CANCEL;
        if (g_equip_item_list_open == 0) {
            g_world_equip_menu_initialized = 0;
            world_menu_reset_selection_results();
            return 0;
        }
        g_equip_item_list_refresh = 1;
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
    if (g_equip_item_list_open == 0) {
        g_equip_item_slot = menu_update_vertical_selection_and_mark_change(WORLD_EQUIPMENT_SLOT_COUNT,
            WORLD_MENU_CURSOR_EQUIP_SLOT, g_menu_input_secondary_repeat, MAIN_SFX_PAGE_SWITCH);
        if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
            world_item_build_category_list(g_unit_selected_index, g_equip_item_slot + 1, g_equip_item_slot,
                g_item_list_entries, ITEM_LIST_MODE_MARK_UNEQUIPPABLE);
            menu_set_vsync_mode_and_event_speed(2);
            g_world_formation_unit_cycle_mode = 1;
            g_equip_item_list_open = 1;
            g_world_change_banner_panel_thread_params.style = 1;
            thread_suspend(WORLD_THREAD_SLOT_UPPER_MENU);
            slot = 0;
            if (g_equip_item_slot != 0) {
                slot = g_equip_item_slot - 1;
            }
            menu_get_selection_record_variant(slot, &x, &y, g_item_list_entries);
            menu_init_scrollable_list(g_item_list_entries, x, y, g_text_section_pointers[TEXT_SECTION_ITEM_NAMES]);
            g_world_menu_script_callbacks[0].value = menu_get_entry_item_equipped_count;
            g_world_menu_script_callbacks[1].value = menu_get_entry_item_count;
            g_world_menu_script_callbacks[2].icon = menu_build_item_type_icon_source;
            g_world_menu_script_callbacks[3].icon = menu_get_entry_item_icon_source;
            g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
            g_equip_item_previewed_cursor = -1;
        }
        equipped = g_world_unit_pointers[g_unit_selected_index]->equipment[g_equip_item_slot];
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
                } else if (world_formation_equip_item_to_unit_slot(g_unit_selected_index, g_equip_item_slot, equip)
                    == 1) {
                    item_adjust_inventory_count(equip, -1);
                    world_formation_stage_selected_unit();
                    g_equip_item_list_refresh = 1;
                    g_sound_queued_effect_id = MAIN_SFX_EQUIP;
                } else {
                    g_sound_queued_effect_id = MAIN_SFX_INVALID;
                }
            }
        }
        if (g_menu_list_entry_count != 0 && g_equip_item_previewed_cursor != g_menu_list_selected_index) {
            g_equip_item_previewed_cursor = g_menu_list_selected_index;
            if (world_formation_equip_item_to_unit_slot(WORLD_FORMATION_PREVIEW_UNIT_INDEX, g_equip_item_slot, item)
                == 1) {
                g_equip_item_preview_active = 1;
                g_menu_preview_stats_thread_params.redraw_request = 1;
                item_calculate_equipment_swap_stat_delta(&g_item_preview_stat_detail,
                    &g_menu_selected_unit_stat_summary, g_world_unit_pointers[g_unit_selected_index]->equipment,
                    g_world_formation_temp_unit->equipment);
                bcopy(g_world_unit_pointers[g_unit_selected_index]->equipment, g_world_formation_temp_unit->equipment,
                    10);
            } else {
                g_equip_item_preview_active = 0;
            }
            g_menu_status_display_thread_params.redraw_request = 1;
        }
        menu_cmd_run_stream_variant(&g_equip_item_menu_script, g_menu_input_primary_repeat);
    }
    if (g_equip_item_list_refresh != 0) {
        g_world_change_banner_panel_thread_params.style = 0;
        g_equip_item_list_open = 0;
        g_world_formation_unit_cycle_mode = 2;
        menu_set_vsync_mode_and_event_speed(0);
        g_equip_item_preview_active = 0;
        g_menu_status_display_thread_params.redraw_request = 1;
        thread_resume(WORLD_THREAD_SLOT_UPPER_MENU);
        slot = 0;
        if (g_equip_item_slot != 0) {
            slot = g_equip_item_slot - 1;
        }
        menu_set_selection_record(slot, g_menu_list_selected_index, g_menu_scroll_base_index, g_item_list_entries);
        g_equip_item_list_refresh = 0;
    }
    world_menu_toggle_preview_stats_window(g_equip_item_preview_active);
    cursor_y = &g_world_equip_item_cursor_point.y;
    *cursor_y = g_equip_item_slot * 16 + 0x90;
    gfx_set_otag_index(0x28);
    /* The mode is an argument expression so &g_world_equip_item_cursor_point is formed before its
       branches and derived from cursor_y (addiu a0,s0,-2), as in the target. */
    menu_update_and_draw_sliding_cursor_variant(&g_world_equip_item_cursor_point, &g_world_equip_item_cursor_anim,
        g_equip_item_list_open != 0 || g_menu_input_lock_mode != 0);
    return 1;
}

/*
 * Run the "best fit" equipment menu: build the suggested set once, then run
 * the confirmation thread. On confirm, return the current equipment to the
 * inventory and equip the suggested set.
 */
s32 world_menu_run_best_fit_equipment(void) {
    /* The target reserves 64 local bytes whose contents are not accessed. */
    s32 unused[16];
    s32 i;
    s16* choice;

    if (g_world_item_best_remove_initialized == 0) {
        g_world_best_fit_confirm_menu.selected_index = 0;
        world_unit_build_best_fit_equipment(g_unit_selected_index, g_item_preview_stat_detail.state.equipment);
        item_calculate_equipment_swap_stat_delta(&g_item_preview_stat_detail, &g_menu_selected_unit_stat_summary,
            g_world_unit_pointers[g_unit_selected_index]->equipment, (u16*)g_item_preview_stat_detail.state.equipment);
        g_item_preview_stat_detail.state.two_hands = unit_is_two_handing_weapon(
            g_item_preview_stat_detail.state.equipment, world_ability_has_two_hands(g_unit_selected_index));
        world_menu_toggle_preview_stats_window(1);
        menu_toggle_stat_preview_panel_thread(1);
        g_menu_preview_stats_thread_params.redraw_request = 1;
        g_menu_status_display_thread_params.redraw_request = 1;
        thread_suspend(WORLD_THREAD_SLOT_UPPER_MENU);
        g_world_formation_unit_cycle_mode = 1;
        g_world_item_best_remove_initialized = 1;
        g_sound_suppressed = 1;
    }
    g_world_item_best_remove_initialized
        = menu_start_managed_thread(THREAD_SCREEN_SLOT_MENU, &g_world_best_fit_confirm_menu);
    if (g_world_item_best_remove_initialized == 0) {
        choice = &g_world_menu_selection_results[11];
        if (*choice == 0) {
            g_sound_queued_effect_id = MAIN_SFX_EQUIP;
        }
        if (*choice == 1) {
            g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
        }
        menu_toggle_stat_preview_panel_thread(0);
        world_menu_toggle_preview_stats_window(0);
        g_menu_status_display_thread_params.redraw_request = 1;
        g_world_formation_unit_cycle_mode = 2;
        if (*choice == 0) {
            for (i = 0; i < WORLD_EQUIPMENT_SLOT_COUNT; i++) {
                item_adjust_inventory_count(g_world_unit_pointers[g_unit_selected_index]->equipment[i], 1);
            }
            for (i = 0; i < WORLD_EQUIPMENT_SLOT_COUNT; i++) {
                g_world_unit_pointers[g_unit_selected_index]->equipment[i]
                    = g_item_preview_stat_detail.state.equipment[i];
                item_adjust_inventory_count(g_item_preview_stat_detail.state.equipment[i], -1);
            }
            world_formation_save_records_to_party();
            world_formation_stage_selected_unit();
        }
        thread_resume(WORLD_THREAD_SLOT_UPPER_MENU);
        g_sound_suppressed = 0;
        return 0;
    }
    return 1;
}

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
        bcopy(g_world_unit_pointers[g_unit_selected_index]->equipment, g_remove_item_equipment,
            sizeof(g_remove_item_equipment));
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
            bcopy(g_world_unit_pointers[g_unit_selected_index]->equipment, g_remove_item_equipment,
                sizeof(g_remove_item_equipment));
            return 1;
        }
        return 1;
    }
    if (g_menu_input_primary_repeat & PSX_PAD_LEFT) {
        g_remove_all_equipment_prompt_active = 1;
        item_calculate_equipment_swap_stat_delta(&g_item_preview_stat_detail, &g_menu_selected_unit_stat_summary,
            g_world_unit_pointers[g_unit_selected_index]->equipment, g_remove_all_equipment_ids);
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
    items = g_remove_item_equipment;
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
            bcopy(g_world_unit_pointers[g_unit_selected_index]->equipment, items, sizeof(g_remove_item_equipment));
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
        preview_item = (u8)g_remove_item_equipment[slot];
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

/*
 * Handle the remove-all-equipment prompt for the selected formation unit.
 *
 * Redraw the five slot markers on alternate updates. Confirm removes occupied
 * slots; Right exits the prompt. Both paths request the corresponding sound.
 * Return 1 while awaiting input and 0 when the prompt has finished.
 */
s32 world_menu_handle_remove_all_equipment(void) {
    u16 value;
    /* The target reserves 64 additional local bytes; their purpose is unknown. */
    s32 unused[16];
    s32 i;
    s32 changed;

    g_menu_help_message_text_id = -1;
    gfx_set_otag_index(0x28);
    g_world_remove_all_equipment_blink_counter = g_world_remove_all_equipment_blink_counter + 1;
    if (g_world_remove_all_equipment_blink_counter & 1) {
        for (i = 0; i < WORLD_EQUIPMENT_SLOT_COUNT; i++) {
            g_world_remove_all_equipment_cursor_point.y = i * 0x10 + 0x90;
            value = 0;
            menu_update_and_draw_sliding_cursor_variant(
                &g_world_remove_all_equipment_cursor_point, &value, g_menu_input_lock_mode);
        }
    }
    if (g_menu_input_primary_repeat & PSX_PAD_RIGHT) {
        g_sound_queued_effect_id = MAIN_SFX_PAGE_SWITCH;
        return 0;
    }
    if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
        changed = 0;
        for (i = 0; i < WORLD_EQUIPMENT_SLOT_COUNT; i++) {
            if (g_world_unit_pointers[g_unit_selected_index]->equipment[i] != ITEM_ID_NOTHING) {
                changed = 1;
                world_formation_equip_item_to_unit_slot(g_unit_selected_index, i, ITEM_ID_NOTHING);
            }
        }
        world_formation_stage_selected_unit();
        if (changed == 0) {
            g_sound_queued_effect_id = MAIN_SFX_INVALID;
        } else {
            g_sound_queued_effect_id = MAIN_SFX_UNEQUIP;
        }
        return 0;
    }
    return 1;
}

/* The on_change callback of g_world_item_action_quantity_range and the prepare callback of
 * g_world_selected_unit_text_binding: nothing to do. */
void world_formation_menu_noop_callback(void) {
}

void world_item_discard_quantity_entry_thread(void) {
    world_menu_number_entry_thread();
}

/* Menu script callback 6 of the item category menu: the selected unit's previewed HP change. */
s32 world_menu_get_selected_unit_hp_bonus(s32 unused_row) {
    return g_menu_selected_unit_stat_summary.gauges[MENU_UNIT_STATUS_GAUGE_HP].bonus;
}

/* Menu script callback 7 of the item category menu: the selected unit's previewed MP change. */
s32 world_menu_get_selected_unit_mp_bonus(s32 unused_row) {
    return g_menu_selected_unit_stat_summary.gauges[MENU_UNIT_STATUS_GAUGE_MP].bonus;
}
