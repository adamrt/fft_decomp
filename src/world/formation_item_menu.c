/* The formation item menu, including equip, best fit, remove and remove-all, quantity discards and the selected unit HP
 * bonus. */

#include "fft/equipment.h"
#include "fft/formation.h"
#include "fft/formation_ability.h"
#include "fft/formation_item.h"
#include "fft/gfx.h"
#include "fft/gfx_primitive.h"
#include "fft/help.h"
#include "fft/item.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/menu_script.h"
#include "fft/menu_selection.h"
#include "fft/message_box.h"
#include "fft/screen.h"
#include "fft/sound.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/unit_panel.h"
#include "psx/libc.h"
#include "psx/pad.h"
#include "psx/types.h"

static s32 formation_item_equip_menu(void);
static s32 formation_item_remove_all_prompt(void);
static s32 formation_item_best_fit_menu(void);
static s32 formation_item_remove_menu(void);

/* The formation screen's Item menu, run each frame: slide the grid in or out, keep the
 * icon menu running, enter Equip, Best Fit, Remove (all refused for a locked unit) or the
 * item list, and come back to the icon menu when that submenu ends. */
void formation_item_menu(void) {
    s32 state = g_formation_current_menu;
    s8 running;
    u8 saved;
    s32 done;
    s32 roster_slot;
    s16* selection;

    if (state == FORMATION_MENU_ITEM_ENTER || state == FORMATION_MENU_ITEM_ENTER_NO_SLIDE) {
        if (g_formation_item_menu_initialized == 0) {
            saved = g_formation_unit_browse_enabled;
            g_formation_unit_banner_at_bottom = 0;
            g_formation_unit_browse_enabled = 0;
            g_formation_item_menu_saved_browse_enabled = saved;
            menu_request_thread_stop(THREAD_SCREEN_SLOT_CHARACTER_STATUS);
            g_formation_item_menu_initialized = 1;
        }
        if (g_formation_current_menu == FORMATION_MENU_ITEM_ENTER) {
            g_formation_item_menu_initialized = formation_step_grid_transition(0);
        } else {
            g_formation_item_menu_initialized = 0;
        }
        if (g_formation_item_menu_initialized == 0) {
            g_formation_current_menu = FORMATION_MENU_ITEM;
        }
        return;
    }
    if (state == FORMATION_MENU_ITEM_LEAVE) {
        running = formation_step_grid_return(1);
        g_formation_item_menu_initialized = running;
        if (running == 0) {
            g_formation_current_menu = FORMATION_MENU_UNIT_GRID;
            g_formation_triangle_menu.selected_index = 0;
        }
        return;
    }
    if (g_formation_item_menu_initialized == 0 && thread_is_running(SCREEN_THREAD_SLOT_UPPER_MENU) == 0) {
        g_formation_item_icon_menu.selected_index = 0;
        g_menu_thread_menu_data = &g_formation_item_icon_menu;
        thread_start(SCREEN_THREAD_SLOT_UPPER_MENU, menu_icon_list_thread);
        thread_set_parameters(SCREEN_THREAD_SLOT_UPPER_MENU, (s32)g_menu_thread_menu_data, 0, 0);
        g_formation_item_menu_unit_index = 0xFF;
        menu_clear_selection_records();
        g_formation_unit_banner_enabled = 1;
        g_formation_item_submenu_exit_pending = 0;
        g_formation_item_menu_initialized = 1;
    }
    if (g_formation_current_menu < FORMATION_MENU_ITEM_LIST) {
        formation_place_portrait_at_rest(g_unit_selected_index);
    }
    if (g_formation_item_menu_unit_index != g_unit_selected_index) {
        g_formation_item_menu_unit_index = g_unit_selected_index;
        if (formation_get_unit_lock(g_unit_selected_index) < 0) {
            g_formation_item_icon_menu.text_id = FORMATION_ITEM_MENU_TEXT_LOCKED;
        } else {
            g_formation_item_icon_menu.text_id = FORMATION_ITEM_MENU_TEXT_UNLOCKED;
        }
        g_screen_text_redraw_request = 1;
    }
    if (g_formation_item_submenu_exit_pending != 0) {
        thread_suspend(SCREEN_THREAD_SLOT_UPPER_MENU_CHILD);
        g_formation_item_submenu_exit_pending = 0;
    }
    if (g_formation_current_menu == FORMATION_MENU_ITEM) {
        g_formation_unit_cycle_mode = 2;
        /* Equip, Best Fit and Remove (rows 0-2) are refused for a locked unit; the item list is not. */
        if (g_screen_selection_results[1] >= 0 && g_screen_selection_results[1] < 3
            && formation_get_unit_lock(g_unit_selected_index) < 0) {
            g_sound_queued_effect_id = SOUND_SFX_INVALID;
            screen_reset_selection_results();
            return;
        }
        selection = &g_screen_selection_results[1];
        if (*selection != -1) {
            g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
            thread_start(SCREEN_THREAD_SLOT_UPPER_MENU_CHILD, thread_wait_forever_by_frames);
            g_formation_current_menu = *selection + FORMATION_MENU_ITEM_EQUIP;
            menu_init_selection_value(SCREEN_CURSOR_EQUIP_SLOT, 0);
            menu_init_selection_value(SCREEN_CURSOR_REMOVE_SLOT, 0);
            menu_init_selection_value(SCREEN_CURSOR_ITEM_TAB, 0);
        }
        running = thread_is_running(SCREEN_THREAD_SLOT_UPPER_MENU);
        g_formation_item_menu_initialized = running;
        if (running == 0) {
            formation_toggle_change_panel(0);
            roster_slot = g_formation_unit_pointers[g_unit_selected_index]->identity.roster_slot;
            formation_build_record_list(0, g_formation_unit_pointers, FORMATION_RECORD_LIST_ALL);
            g_unit_selected_index = formation_find_by_roster_slot(roster_slot);
            g_formation_current_menu = FORMATION_MENU_ITEM_LEAVE;
            g_formation_unit_browse_enabled = g_formation_item_menu_saved_browse_enabled;
            g_formation_unit_banner_enabled = g_formation_saved_banner_enabled;
        }
        return;
    }
    switch (g_formation_current_menu) {
    case FORMATION_MENU_ITEM_EQUIP:
        done = formation_item_equip_menu();
        break;
    case FORMATION_MENU_ITEM_BEST_FIT:
        done = formation_item_best_fit_menu();
        break;
    case FORMATION_MENU_ITEM_REMOVE:
        done = formation_item_remove_menu();
        break;
    case FORMATION_MENU_ITEM_LIST:
        done = equipment_update_menu();
        break;
    }
    if (done == 0) {
        g_formation_item_menu_unit_index = 0xFF;
        formation_stage_selected_unit();
        g_formation_current_menu = FORMATION_MENU_ITEM;
        g_formation_item_submenu_exit_pending = 1;
    }
}

/* Run the formation "Equip" item menu: step the slot cursor, open the item
 * list for the selected slot, equip the highlighted item and keep the stat
 * preview in sync. Returns 0 when the menu is canceled, 1 otherwise.
 */
static s32 formation_item_equip_menu(void) {
    s16 x;
    s16 y;
    u8 locked;
    s32 slot;
    s32 item;
    /* s16: the conversion from item keeps CSE from folding the copy away
     * (the target moves item into a second saved register here). */
    s16 equip;
    u16 equipped;
    s32 value;
    s16* cursor_y;

    if (g_formation_item_equip_menu_initialized == 0) {
        g_formation_item_equip_menu_initialized = 1;
        g_formation_item_previewed_cursor = -1;
        g_formation_item_list_open = 0;
        g_formation_unit_cycle_mode = 2;
        g_formation_item_slot = 0;
        g_formation_item_list_refresh = 0;
        g_formation_item_preview_active = 0;
    }

    /* Cross leaves the menu from the slot cursor, or closes the open list below. */
    if (g_menu_input_primary_repeat & PSX_PAD_CROSS) {
        g_sound_queued_effect_id = SOUND_SFX_CANCEL;
        if (g_formation_item_list_open == 0) {
            g_formation_item_equip_menu_initialized = 0;
            screen_reset_selection_results();
            return 0;
        }
        g_formation_item_list_refresh = 1;
    }

    /* UNLOCKED is 1, so fold it to 0: any other nonzero result is a locked unit. */
    locked = formation_get_unit_lock(g_unit_selected_index);
    if (locked == FORMATION_UNIT_UNLOCKED) {
        locked = 0;
    }
    if (locked != 0) {
        formation_toggle_preview_stats_window(0);
        g_help_message_text_id = 0;
        if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
            g_sound_queued_effect_id = SOUND_SFX_INVALID;
        }
        return 1;
    }

    if (g_formation_item_list_open == 0) {
        g_formation_item_slot = menu_update_vertical_selection_and_mark_change(
            MENU_EQUIPMENT_SLOT_COUNT, SCREEN_CURSOR_EQUIP_SLOT, g_menu_input_secondary_repeat, SOUND_SFX_PAGE_SWITCH);
        if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
            formation_item_build_list(g_unit_selected_index, g_formation_item_slot + 1, g_formation_item_slot,
                g_item_list_entries, ITEM_LIST_MODE_MARK_UNEQUIPPABLE);

            menu_set_vsync_mode_and_event_speed(2);
            g_formation_unit_cycle_mode = 1;
            g_formation_item_list_open = 1;
            g_formation_change_banner_thread_params.style = 1;
            thread_suspend(SCREEN_THREAD_SLOT_UPPER_MENU);
            /* Both hand slots share selection record 0, so the record is the slot less one. */
            slot = 0;
            if (g_formation_item_slot != 0) {
                slot = g_formation_item_slot - 1;
            }
            menu_get_selection_record(slot, &x, &y, g_item_list_entries);
            menu_init_scrollable_list(g_item_list_entries, x, y, g_text_section_pointers[TEXT_SECTION_ITEM_NAMES]);
            g_screen_script_callbacks[0].value = equipment_get_row_equipped_count;
            g_screen_script_callbacks[1].value = equipment_get_row_item_count;
            g_screen_script_callbacks[2].icon = item_build_type_icon_source;
            g_screen_script_callbacks[3].icon = equipment_get_row_icon_source;
            g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
            g_formation_item_previewed_cursor = -1;
        }

        equipped = g_formation_unit_pointers[g_unit_selected_index]->equipment[g_formation_item_slot];
        value = -1;
        if (equipped != ITEM_ID_NOTHING) {
            value = equipped + HELP_TEXT_ID_ITEM_BASE;
        }
        g_help_message_text_id = value;
    } else {
        g_help_message_text_id = g_menu_list_entry_count != 0
            ? (g_item_list_entries[g_menu_list_selected_index] & ITEM_ENTRY_ID_MASK) + HELP_TEXT_ID_ITEM_BASE
            : -1;
        item = g_menu_list_entry_count != 0 ? g_item_list_entries[g_menu_list_selected_index] & ITEM_ENTRY_ID_MASK : 0;

        if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
            if (g_menu_list_entry_count == 0
                || (g_item_list_entries[g_menu_list_selected_index] & ITEM_ENTRY_UNEQUIPPABLE)) {
                g_sound_queued_effect_id = SOUND_SFX_INVALID;
            } else {
                equip = item;
                /* A zero adjustment only reads the stock count. */
                if (item_adjust_inventory_count(equip, 0) == 0) {
                    message_box_show_with_sound(0xc003, SOUND_SFX_ERROR_MESSAGE);
                } else if (formation_item_equip(g_unit_selected_index, g_formation_item_slot, equip) == 1) {
                    item_adjust_inventory_count(equip, -1);
                    formation_stage_selected_unit();
                    g_formation_item_list_refresh = 1;
                    g_sound_queued_effect_id = SOUND_SFX_EQUIP;
                } else {
                    g_sound_queued_effect_id = SOUND_SFX_INVALID;
                }
            }
        }

        /* Try the item on the preview unit, take the stat delta, then restore the preview unit's equipment. */
        if (g_menu_list_entry_count != 0 && g_formation_item_previewed_cursor != g_menu_list_selected_index) {
            g_formation_item_previewed_cursor = g_menu_list_selected_index;
            if (formation_item_equip(FORMATION_PREVIEW_UNIT_INDEX, g_formation_item_slot, item) == 1) {
                g_formation_item_preview_active = 1;
                g_unit_panel_preview_stats_thread_params.redraw_request = 1;
                equipment_calculate_total_swap_delta(&g_item_preview_stat_detail,
                    &g_unit_panel_selected_unit_stat_summary,
                    g_formation_unit_pointers[g_unit_selected_index]->equipment, g_formation_temp_unit->equipment);
                bcopy(g_formation_unit_pointers[g_unit_selected_index]->equipment, g_formation_temp_unit->equipment,
                    sizeof(g_formation_temp_unit->equipment));
            } else {
                g_formation_item_preview_active = 0;
            }
            g_unit_panel_status_display_thread_params.redraw_request = 1;
        }
        menu_script_run(&g_formation_item_menu_script, g_menu_input_primary_repeat);
    }

    if (g_formation_item_list_refresh != 0) {
        g_formation_change_banner_thread_params.style = 0;
        g_formation_item_list_open = 0;
        g_formation_unit_cycle_mode = 2;
        menu_set_vsync_mode_and_event_speed(0);
        g_formation_item_preview_active = 0;
        g_unit_panel_status_display_thread_params.redraw_request = 1;
        thread_resume(SCREEN_THREAD_SLOT_UPPER_MENU);
        slot = 0;
        if (g_formation_item_slot != 0) {
            slot = g_formation_item_slot - 1;
        }
        menu_set_selection_record(slot, g_menu_list_selected_index, g_menu_scroll_base_index, g_item_list_entries);
        g_formation_item_list_refresh = 0;
    }

    formation_toggle_preview_stats_window(g_formation_item_preview_active);
    cursor_y = &g_formation_item_equip_item_cursor_point.y;
    *cursor_y = g_formation_item_slot * 16 + 0x90;
    gfx_set_otag_index(0x28);
    /* The mode is an argument expression so &g_formation_item_equip_item_cursor_point is formed before its
     * branches and derived from cursor_y (addiu a0,s0,-2), as in the target. */
    screen_draw_sliding_cursor(&g_formation_item_equip_item_cursor_point, &g_formation_item_equip_item_cursor_anim,
        g_formation_item_list_open != 0 || g_menu_input_lock_mode != 0);
    return 1;
}

/*
 * Run the "best fit" equipment menu: build the suggested set once, then run
 * the confirmation thread. On confirm, return the current equipment to the
 * inventory and equip the suggested set.
 */
static s32 formation_item_best_fit_menu(void) {
    /* The target reserves 64 local bytes whose contents are not accessed. */
    s32 unused[16];
    s32 i;
    s16* choice;

    if (g_formation_item_best_remove_initialized == 0) {
        g_formation_item_best_fit_confirm_menu.selected_index = 0;
        formation_item_build_best_fit(g_unit_selected_index, g_item_preview_stat_detail.state.equipment);
        equipment_calculate_total_swap_delta(&g_item_preview_stat_detail, &g_unit_panel_selected_unit_stat_summary,
            g_formation_unit_pointers[g_unit_selected_index]->equipment,
            (u16*)g_item_preview_stat_detail.state.equipment);
        g_item_preview_stat_detail.state.two_hands = unit_is_two_handing_weapon(
            g_item_preview_stat_detail.state.equipment, formation_ability_has_two_hands(g_unit_selected_index));
        formation_toggle_preview_stats_window(1);
        formation_toggle_stat_preview_panel(1);
        g_unit_panel_preview_stats_thread_params.redraw_request = 1;
        g_unit_panel_status_display_thread_params.redraw_request = 1;
        thread_suspend(SCREEN_THREAD_SLOT_UPPER_MENU);
        g_formation_unit_cycle_mode = 1;
        g_formation_item_best_remove_initialized = 1;
        g_sound_suppressed = 1;
    }
    g_formation_item_best_remove_initialized
        = menu_start_managed_thread(THREAD_SCREEN_SLOT_MENU, &g_formation_item_best_fit_confirm_menu);
    if (g_formation_item_best_remove_initialized == 0) {
        choice = &g_screen_selection_results[11];
        if (*choice == 0) {
            g_sound_queued_effect_id = SOUND_SFX_EQUIP;
        }
        if (*choice == 1) {
            g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
        }
        formation_toggle_stat_preview_panel(0);
        formation_toggle_preview_stats_window(0);
        g_unit_panel_status_display_thread_params.redraw_request = 1;
        g_formation_unit_cycle_mode = 2;
        if (*choice == 0) {
            for (i = 0; i < MENU_EQUIPMENT_SLOT_COUNT; i++) {
                item_adjust_inventory_count(g_formation_unit_pointers[g_unit_selected_index]->equipment[i], 1);
            }
            for (i = 0; i < MENU_EQUIPMENT_SLOT_COUNT; i++) {
                g_formation_unit_pointers[g_unit_selected_index]->equipment[i]
                    = g_item_preview_stat_detail.state.equipment[i];
                item_adjust_inventory_count(g_item_preview_stat_detail.state.equipment[i], -1);
            }
            formation_save_records();
            formation_stage_selected_unit();
        }
        thread_resume(SCREEN_THREAD_SLOT_UPPER_MENU);
        g_sound_suppressed = 0;
        return 0;
    }
    return 1;
}

/* Run the Remove menu: Circle unequips the item in the selected slot (it returns to the
 * inventory), Left opens the remove-all prompt and Cross leaves (returning 0; 1 otherwise).
 * The stat preview shows the effect of removing the highlighted item. A locked unit refuses
 * Circle. */
static s32 formation_item_remove_menu(void) {
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

    if (g_formation_item_remove_unit_index != g_unit_selected_index) {
        g_formation_item_best_remove_initialized = 0;
        g_formation_item_remove_unit_index = g_unit_selected_index;
        g_menu_input_secondary_repeat = g_formation_item_remove_saved_secondary_input;
        g_menu_input_primary_repeat = g_formation_item_remove_saved_primary_input;
        g_menu_input_newly_pressed = g_formation_item_remove_saved_newly_pressed;
        formation_update_selection();
        menu_input_clear_state();
    }
    g_formation_item_remove_saved_secondary_input = g_menu_input_secondary_repeat;
    g_formation_item_remove_saved_primary_input = g_menu_input_primary_repeat;
    g_formation_item_remove_saved_newly_pressed = g_menu_input_newly_pressed;
    if (g_formation_item_best_remove_initialized == 0) {
        bcopy(g_formation_unit_pointers[g_unit_selected_index]->equipment, g_equipment_remove_item_equipment,
            sizeof(g_equipment_remove_item_equipment));
        g_equipment_remove_item_previewed_slot = -1;
        g_formation_unit_cycle_mode = 2;
        g_equipment_remove_all_equipment_prompt_active = 0;
        g_formation_item_best_remove_initialized = 1;
        g_formation_item_remove_unit_index = g_unit_selected_index;
    }
    if (g_menu_input_primary_repeat & PSX_PAD_CROSS) {
        formation_toggle_preview_stats_window(0);
        g_formation_item_best_remove_initialized = 0;
        g_sound_queued_effect_id = SOUND_SFX_CANCEL;
        return 0;
    }
    locked = formation_get_unit_lock(g_unit_selected_index);
    if (locked == FORMATION_UNIT_UNLOCKED) {
        locked = 0;
    }
    if (locked != 0) {
        g_help_message_text_id = 0;
        formation_toggle_preview_stats_window(0);
        if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
            g_sound_queued_effect_id = SOUND_SFX_INVALID;
        }
        return 1;
    }
    if (g_equipment_remove_all_equipment_prompt_active != 0) {
        g_equipment_remove_all_equipment_prompt_active = formation_item_remove_all_prompt();
        if (g_equipment_remove_all_equipment_prompt_active == 0) {
            g_equipment_remove_item_previewed_slot = -2;
            bcopy(g_formation_unit_pointers[g_unit_selected_index]->equipment, g_equipment_remove_item_equipment,
                sizeof(g_equipment_remove_item_equipment));
            return 1;
        }
        return 1;
    }
    if (g_menu_input_primary_repeat & PSX_PAD_LEFT) {
        g_equipment_remove_all_equipment_prompt_active = 1;
        equipment_calculate_total_swap_delta(&g_item_preview_stat_detail, &g_unit_panel_selected_unit_stat_summary,
            g_formation_unit_pointers[g_unit_selected_index]->equipment, g_equipment_remove_all_equipment_ids);
        g_unit_panel_status_display_thread_params.redraw_request = 1;
        g_unit_panel_preview_stats_thread_params.redraw_request = 1;
        formation_toggle_preview_stats_window(1);
        g_sound_queued_effect_id = SOUND_SFX_PAGE_SWITCH;
    }
    slot = menu_update_vertical_selection_and_mark_change(
        MENU_EQUIPMENT_SLOT_COUNT, SCREEN_CURSOR_REMOVE_SLOT, g_menu_input_secondary_repeat, SOUND_SFX_PAGE_SWITCH);
    cursor_y = &g_formation_item_remove_cursor_point.y;
    index = slot;
    *cursor_y = index * 16 + 0x90;
    gfx_set_otag_index(0x28);
    screen_draw_sliding_cursor(
        &g_formation_item_remove_cursor_point, &g_formation_item_remove_cursor_anim, g_menu_input_lock_mode);
    items = g_equipment_remove_item_equipment;
    selected_item = &items[index];
    item = *selected_item;
    value = -1;
    if (item != ITEM_ID_NOTHING) {
        value = item + HELP_TEXT_ID_ITEM_BASE;
    }
    g_help_message_text_id = value;
    if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
        if (*selected_item == ITEM_ID_NOTHING) {
            g_sound_queued_effect_id = SOUND_SFX_INVALID;
        } else {
            formation_item_equip(g_unit_selected_index, slot, ITEM_ID_NOTHING);
            bcopy(g_formation_unit_pointers[g_unit_selected_index]->equipment, items,
                sizeof(g_equipment_remove_item_equipment));
            formation_stage_selected_unit();
            g_sound_queued_effect_id = SOUND_SFX_UNEQUIP;
            g_equipment_remove_item_previewed_slot = -2;
        }
    }
    previous = g_equipment_remove_item_previewed_slot;
    if (previous != slot) {
        if (previous != -1) {
            g_unit_panel_status_display_thread_params.redraw_request = 1;
        }
        g_equipment_remove_item_previewed_slot = slot;
        preview_item = g_equipment_remove_item_equipment[slot];
        if (preview_item != ITEM_ID_NOTHING) {
            formation_item_equip(FORMATION_PREVIEW_UNIT_INDEX, slot, ITEM_ID_NOTHING);
            equipment_calculate_total_swap_delta(&g_item_preview_stat_detail, &g_unit_panel_selected_unit_stat_summary,
                g_formation_unit_pointers[g_unit_selected_index]->equipment, g_formation_temp_unit->equipment);
            bcopy(g_formation_unit_pointers[g_unit_selected_index]->equipment, g_formation_temp_unit->equipment,
                sizeof(g_formation_temp_unit->equipment));
            g_unit_panel_preview_stats_thread_params.redraw_request = 1;
        }
        formation_toggle_preview_stats_window(preview_item != ITEM_ID_NOTHING);
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
static s32 formation_item_remove_all_prompt(void) {
    u16 value;
    /* The target reserves 64 additional local bytes; their purpose is unknown. */
    s32 unused[16];
    s32 i;
    s32 changed;

    g_help_message_text_id = -1;
    gfx_set_otag_index(0x28);
    g_formation_item_remove_all_blink_counter = g_formation_item_remove_all_blink_counter + 1;
    if (g_formation_item_remove_all_blink_counter & 1) {
        for (i = 0; i < MENU_EQUIPMENT_SLOT_COUNT; i++) {
            g_formation_item_remove_all_cursor_point.y = i * 0x10 + 0x90;
            value = 0;
            screen_draw_sliding_cursor(&g_formation_item_remove_all_cursor_point, &value, g_menu_input_lock_mode);
        }
    }
    if (g_menu_input_primary_repeat & PSX_PAD_RIGHT) {
        g_sound_queued_effect_id = SOUND_SFX_PAGE_SWITCH;
        return 0;
    }
    if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
        changed = 0;
        for (i = 0; i < MENU_EQUIPMENT_SLOT_COUNT; i++) {
            if (g_formation_unit_pointers[g_unit_selected_index]->equipment[i] != ITEM_ID_NOTHING) {
                changed = 1;
                formation_item_equip(g_unit_selected_index, i, ITEM_ID_NOTHING);
            }
        }
        formation_stage_selected_unit();
        if (changed == 0) {
            g_sound_queued_effect_id = SOUND_SFX_INVALID;
        } else {
            g_sound_queued_effect_id = SOUND_SFX_UNEQUIP;
        }
        return 0;
    }
    return 1;
}

/* The on_change callback of g_formation_item_action_quantity_range and the prepare callback of
 * g_formation_selected_unit_text_binding: nothing to do. */
void formation_noop_callback(void) {
}

/* Thread entry of the quantity row of g_item_action_menu: a plain call to
 * screen_number_entry_thread. */
void formation_item_discard_quantity_thread(void) {
    screen_number_entry_thread();
}

/* Menu script callback 6 of the item category menu: the selected unit's previewed HP change. */
s32 formation_item_get_hp_bonus(s32 unused_row) {
    return g_unit_panel_selected_unit_stat_summary.gauges[MENU_UNIT_STATUS_GAUGE_HP].bonus;
}

/* Menu script callback 7 of the item category menu: the selected unit's previewed MP change. */
s32 formation_item_get_mp_bonus(s32 unused_row) {
    return g_unit_panel_selected_unit_stat_summary.gauges[MENU_UNIT_STATUS_GAUGE_MP].bonus;
}
