/* Equip screen state, the equip and remove modes, the clear-all prompt and the preview HP bonus. */

#include "fft/equipment.h"
#include "fft/gfx.h"
#include "fft/gfx_primitive.h"
#include "fft/help.h"
#include "fft/item.h"
#include "fft/menu.h"
#include "fft/menu_cursor.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/menu_script.h"
#include "fft/menu_selection.h"
#include "fft/sound.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/unit_panel.h"
#include "psx/libc.h"
#include "psx/pad.h"
#include "psx/types.h"

extern s8 g_equipment_equip_mode_initialized;
extern s8 g_equipment_remove_mode_initialized;

/* EQUIP's 0x3c-byte menu records: the main commands, the item actions and
 * (in equipment_run_type_order_mode) the item-type order menus. Each is
 * started through menu_start_managed_thread, which makes it the current
 * g_menu_thread_menu_data. */
extern menu_entry_t g_equipment_command_menu;
extern volatile u16 g_equipment_clear_all_prompt_frame;
extern s8 g_equipment_item_slot;
extern s8 g_equipment_item_list_open;
extern s8 g_equipment_item_list_refresh;
extern s8 g_equipment_item_preview_active;
extern s16 g_equipment_item_previewed_cursor;
extern s8 g_equipment_thread_running;
extern u8 g_equipment_thread_idle_stop_pending;

/* Byte-aligned display streams, including the terminating opcode. */
extern u8 g_equipment_item_menu_script[198];

/* Cursor anchors and their packed active/frame states belong to these modes. */
extern point16_t g_equipment_equip_cursor_position;
extern u16 g_equipment_equip_cursor_state;
extern point16_t g_equipment_remove_cursor_position;
extern u16 g_equipment_remove_cursor_state;
extern point16_t g_equipment_remove_all_cursor_position;

/* EQUIP's screen message 9 (TEXT_SECTION_OVERLAY_SCREEN): none of the item is left. */
enum { EQUIPMENT_TEXT_ID_OUT_OF_STOCK = (TEXT_SECTION_OVERLAY_SCREEN << TEXT_ID_SECTION_SHIFT) + 9 };

static void equipment_update_state(void);
static s32 equipment_run_equip_mode(void);
static s32 equipment_run_remove_mode(void);
static s32 equipment_update_remove_all_prompt(void);

/* State 0 initializes before dispatch; the five other slots share the frame updater. */
void (*g_equipment_screen_state_handlers[6])(void) = { 0, equipment_update_state, equipment_update_state,
    equipment_update_state, equipment_update_state, equipment_update_state };

/* The g_equipment_screen_state_handlers entry of every state past INIT: keep the command
 * menu thread running, enter the mode its picked row names (row + EQUIPMENT_STATE_EQUIP;
 * row 1 would be state 3, which has no mode, and is refused), run that mode each frame and
 * return to the command menu when it ends. */
static void equipment_update_state(void) {
    s32 result;
    s32 state;
    s32 mode;
    s16* value_ptr;

    if (g_equipment_thread_running == 0) {
        if (thread_is_running(EQUIPMENT_THREAD_SLOT_COMMAND_MENU) == 0) {
            g_equipment_command_menu.selected_index = 0;
            g_menu_thread_menu_data = &g_equipment_command_menu;
            thread_start(EQUIPMENT_THREAD_SLOT_COMMAND_MENU, menu_icon_list_thread);
            thread_set_parameters(EQUIPMENT_THREAD_SLOT_COMMAND_MENU, (s32)g_menu_thread_menu_data, 0, 0);
            g_equipment_thread_idle_stop_pending = 0;
            g_equipment_thread_running = 1;
        }
    }
    if (g_equipment_thread_idle_stop_pending != 0) {
        thread_suspend(EQUIPMENT_THREAD_SLOT_COMMAND_MENU_CHILD);
        g_equipment_thread_idle_stop_pending = 0;
    }
    state = g_equipment_screen_state;
    if (state == EQUIPMENT_STATE_COMMAND_MENU) {
        value_ptr = &g_equipment_result_rows[0];
        mode = *value_ptr;
        /* Each row's help text, from entry 0x29 of HELP_TEXT_SECTION_MENU_COMMANDS (0x1000) on. */
        g_help_message_text_id = g_equipment_command_menu.selected_index + 0x1029;
        if (mode == 1) {
            g_sound_queued_effect_id = SOUND_SFX_INVALID;
            equipment_reset_selection();
        } else if (mode != -1) {
            thread_start(EQUIPMENT_THREAD_SLOT_COMMAND_MENU_CHILD, thread_wait_forever_by_frames);
            g_equipment_screen_state = *value_ptr + EQUIPMENT_STATE_EQUIP;
            menu_set_selection_value(0, 0);
            menu_set_selection_value(1, 0);
            menu_set_selection_value(2, 0);
            equipment_reset_selection();
        }
        g_equipment_thread_running = thread_is_running(EQUIPMENT_THREAD_SLOT_COMMAND_MENU);
        if (g_equipment_thread_running == 0) {
            g_equipment_screen_state = EQUIPMENT_STATE_CLOSED;
        }
        return;
    }
    switch (state) {
    case EQUIPMENT_STATE_EQUIP:
        result = equipment_run_equip_mode();
        break;
    case EQUIPMENT_STATE_REMOVE:
        result = equipment_run_remove_mode();
        break;
    case EQUIPMENT_STATE_ITEM_LIST:
        result = equipment_update_menu();
        break;
    }
    if (result == 0) {
        g_equipment_screen_state = EQUIPMENT_STATE_COMMAND_MENU;
        g_equipment_thread_idle_stop_pending = 1;
    }
}

/* Runs the slot list and, once a slot is confirmed, its item list.
 *
 * Confirming an item checks the inventory count, equips it and decrements
 * the count; moving the highlight equips the item on the scratch unit record
 * g_equipment_unit_data[1] so the stat panel can preview the change. The WORLD
 * formation menu formation_item_equip_menu is the same routine plus an
 * equipment-lock check.
 *
 * The if/else spelling of the slot fallback is required: it keeps CSE from
 * reusing the stored slot for the category argument (the target reloads it)
 * and schedules the unit load ahead of the slot test. */
static s32 equipment_run_equip_mode(void) {
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

    if (g_equipment_equip_mode_initialized == 0) {
        g_equipment_equip_mode_initialized = 1;
        g_equipment_item_previewed_cursor = -1;
        g_equipment_item_list_open = 0;
        g_menu_list_selected_index = 0;
        g_equipment_item_preview_active = 0;
    }
    if (g_menu_input_primary_repeat & PSX_PAD_CROSS) {
        g_sound_queued_effect_id = SOUND_SFX_CANCEL;
        if (g_equipment_item_list_open == 0) {
            g_equipment_equip_mode_initialized = 0;
            equipment_reset_selection();
            return 0;
        }
        g_equipment_item_list_refresh = 1;
    }
    if (g_equipment_item_list_open == 0) {
        /* The help text is the slot item's entry in HELP_TEXT_SECTION_ITEMS. */
        entry = g_equipment_unit_data[g_equipment_unit_selected_index]->equipment[g_equipment_item_slot];
        help_id = -1;
        if (entry != 0) {
            help_id = entry + HELP_TEXT_ID_ITEM_BASE;
        }
        g_help_message_text_id = help_id;
        selected_slot = menu_update_vertical_selection_and_mark_change(
            5, 0, g_menu_input_secondary_repeat, SOUND_SFX_PAGE_SWITCH);
        g_equipment_item_slot = selected_slot;
        if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
            unit = g_equipment_unit_selected_index;
            if (selected_slot == 0) {
                slot = 1;
            } else {
                slot = selected_slot;
            }
            equipment_build_item_list(
                unit, slot, g_equipment_item_slot, g_item_list_entries, ITEM_LIST_MODE_MARK_UNEQUIPPABLE);
            g_equipment_item_list_open = 1;
            g_equipment_panel_params.style = 1;
            thread_suspend(EQUIPMENT_THREAD_SLOT_COMMAND_MENU);
            menu_get_selection_record(
                g_equipment_item_slot != 0 ? g_equipment_item_slot - 1 : 0, &selected, &scroll, g_item_list_entries);
            menu_init_scrollable_list(
                g_item_list_entries, selected, scroll, g_text_section_pointers[TEXT_SECTION_ITEM_NAMES]);
            g_menu_row_callbacks[0].value = equipment_get_row_equipped_count;
            g_menu_row_callbacks[1].value = equipment_get_row_item_count;
            g_menu_row_callbacks[2].icon = item_build_type_icon_source;
            g_menu_row_callbacks[3].icon = equipment_get_row_icon_source;
            g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
            g_equipment_item_previewed_cursor = -1;
        }
    } else {
        g_help_message_text_id = g_menu_list_entry_count != 0
            ? (g_item_list_entries[g_menu_list_selected_index] & ITEM_ENTRY_ID_MASK) + HELP_TEXT_ID_ITEM_BASE
            : -1;
        item = g_menu_list_entry_count != 0 ? g_item_list_entries[g_menu_list_selected_index] & ITEM_ENTRY_ID_MASK : 0;
        if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
            if (g_menu_list_entry_count != 0
                && (g_item_list_entries[g_menu_list_selected_index] & ITEM_ENTRY_UNEQUIPPABLE) == 0) {
                item_id = item;
                if (item_adjust_inventory_count(item_id, 0) == 0) {
                    equipment_show_message(EQUIPMENT_TEXT_ID_OUT_OF_STOCK, SOUND_SFX_ERROR_MESSAGE);
                } else if (equipment_set_slot_item(g_equipment_unit_selected_index, g_equipment_item_slot, item_id)
                    == EQUIPMENT_SLOT_ITEM_ALLOWED) {
                    item_adjust_inventory_count(item_id, -1);
                    equipment_load_selected_unit();
                    g_equipment_item_list_refresh = 1;
                    g_sound_queued_effect_id = SOUND_SFX_EQUIP;
                } else {
                    g_sound_queued_effect_id = SOUND_SFX_INVALID;
                }
            } else {
                g_sound_queued_effect_id = SOUND_SFX_INVALID;
            }
        }
        /* Preview the highlighted item on the scratch record g_equipment_unit_data[1]. */
        if (g_menu_list_entry_count != 0) {
            index = g_menu_list_selected_index;
            if (g_equipment_item_previewed_cursor != index) {
                g_equipment_item_previewed_cursor = index;
                result = equipment_set_slot_item(1, g_equipment_item_slot, item);
                if (result == EQUIPMENT_SLOT_ITEM_ALLOWED) {
                    g_equipment_item_preview_active = 1;
                    g_unit_panel_preview_stats_thread_params.redraw_request = result;
                    equipment_calculate_total_swap_delta(&g_item_preview_stat_detail,
                        &g_unit_panel_selected_unit_stat_summary,
                        g_equipment_unit_data[g_equipment_unit_selected_index]->equipment,
                        g_equipment_unit_data[1]->equipment);
                    bcopy(g_equipment_unit_data[g_equipment_unit_selected_index]->equipment,
                        g_equipment_unit_data[1]->equipment, sizeof(g_equipment_unit_data[1]->equipment));
                } else {
                    g_equipment_item_preview_active = 0;
                }
                g_unit_panel_unit_status_panel_params.redraw_request = 1;
            }
        }
        menu_script_run(g_equipment_item_menu_script, g_menu_input_primary_repeat);
    }
    if (g_equipment_item_list_refresh != 0) {
        g_equipment_panel_params.style = 0;
        g_equipment_item_list_open = 0;
        g_equipment_item_preview_active = 0;
        g_unit_panel_unit_status_panel_params.redraw_request = 1;
        thread_resume(EQUIPMENT_THREAD_SLOT_COMMAND_MENU);
        menu_set_selection_record(g_equipment_item_slot != 0 ? g_equipment_item_slot - 1 : 0,
            g_menu_list_selected_index, g_menu_scroll_base_index, g_item_list_entries);
        g_equipment_item_list_refresh = 0;
    }
    unit_panel_toggle_item_numbers_thread(g_equipment_item_preview_active);
    g_equipment_equip_cursor_position.y = g_equipment_item_slot * 16 + 0x90;
    gfx_set_otag_index(0x28);
    menu_update_and_draw_animated_cursor(&g_equipment_equip_cursor_position, &g_equipment_equip_cursor_state,
        g_equipment_item_list_open != 0 || g_menu_input_lock_mode != 0);
    return 1;
}

/* One frame of Remove mode: Circle takes off the item in the chosen slot unless it is
 * locked, Left offers to remove everything, and the stats preview the highlighted slot
 * empty. Returns 0 when Cross leaves the mode, else 1. */
static s32 equipment_run_remove_mode(void) {
    /* Never referenced; the target frame reserves these bytes. */
    u8 unused[0x48];
    s32 i;
    u32 input;
    s32 selection;
    u8 has_item;

    if (g_equipment_remove_mode_initialized == 0) {
        for (i = 0; i < 5; i++) {
            g_equipment_remove_item_equipment[i] = g_equipment_unit_data[g_equipment_unit_selected_index]->equipment[i];
        }
        g_equipment_remove_item_previewed_slot = -1;
        g_equipment_remove_all_equipment_prompt_active = 0;
        g_equipment_remove_mode_initialized = 1;
    }
    input = g_menu_input_primary_repeat;
    if (input & PSX_PAD_CROSS) {
        unit_panel_toggle_item_numbers_thread(0);
        g_unit_panel_unit_status_panel_params.redraw_request = 1;
        g_equipment_remove_mode_initialized = 0;
        g_sound_queued_effect_id = SOUND_SFX_CANCEL;
        return 0;
    }
    if (g_equipment_remove_all_equipment_prompt_active != 0) {
        g_equipment_remove_all_equipment_prompt_active = equipment_update_remove_all_prompt();
        if (g_equipment_remove_all_equipment_prompt_active == 0) {
            g_equipment_remove_item_previewed_slot = -2;
            bcopy(g_equipment_unit_data[g_equipment_unit_selected_index]->equipment, g_equipment_remove_item_equipment,
                sizeof(g_equipment_remove_item_equipment));
        }
        return 1;
    }
    if (input & PSX_PAD_LEFT) {
        g_equipment_remove_all_equipment_prompt_active = 1;
        equipment_calculate_total_swap_delta(&g_item_preview_stat_detail, &g_unit_panel_selected_unit_stat_summary,
            g_equipment_unit_data[g_equipment_unit_selected_index]->equipment, g_equipment_remove_all_equipment_ids);
        g_unit_panel_unit_status_panel_params.redraw_request = 1;
        g_unit_panel_preview_stats_thread_params.redraw_request = 1;
        unit_panel_toggle_item_numbers_thread(1);
        g_sound_queued_effect_id = SOUND_SFX_PAGE_SWITCH;
    }
    selection
        = menu_update_vertical_selection_and_mark_change(5, 1, g_menu_input_secondary_repeat, SOUND_SFX_PAGE_SWITCH);
    g_equipment_remove_cursor_position.y = ((selection & 0xFF) << 4) + 0x90;
    gfx_set_otag_index(0x28);
    menu_update_and_draw_animated_cursor(
        &g_equipment_remove_cursor_position, &g_equipment_remove_cursor_state, g_menu_input_lock_mode);

    g_help_message_text_id = g_equipment_remove_item_equipment[selection & 0xFF] != 0
        ? g_equipment_remove_item_equipment[selection & 0xFF] + HELP_TEXT_ID_ITEM_BASE
        : -1;
    if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
        if (g_equipment_remove_item_equipment[selection & 0xFF] == 0
            || equipment_set_slot_item(g_equipment_unit_selected_index, (u8)selection, 0)
                == EQUIPMENT_SLOT_ITEM_CURRENT_LOCKED) {
            g_sound_queued_effect_id = SOUND_SFX_INVALID;
        } else {
            bcopy(g_equipment_unit_data[g_equipment_unit_selected_index]->equipment, g_equipment_remove_item_equipment,
                sizeof(g_equipment_remove_item_equipment));
            equipment_load_selected_unit();
            g_sound_queued_effect_id = SOUND_SFX_UNEQUIP;
            g_equipment_remove_item_previewed_slot = -1;
        }
    }
    if (g_equipment_remove_item_previewed_slot != (selection & 0xFF)) {
        g_equipment_remove_item_previewed_slot = selection;
        has_item = g_equipment_remove_item_equipment[selection & 0xFF];
        if (has_item != 0) {
            equipment_set_slot_item(1, (u8)selection, 0);
            equipment_calculate_total_swap_delta(&g_item_preview_stat_detail, &g_unit_panel_selected_unit_stat_summary,
                g_equipment_unit_data[g_equipment_unit_selected_index]->equipment, g_equipment_unit_data[1]->equipment);
            bcopy(g_equipment_unit_data[g_equipment_unit_selected_index]->equipment,
                g_equipment_unit_data[1]->equipment, sizeof(g_equipment_unit_data[1]->equipment));
            g_unit_panel_preview_stats_thread_params.redraw_request = 1;
        }
        g_unit_panel_unit_status_panel_params.redraw_request = 1;
        unit_panel_toggle_item_numbers_thread(has_item != 0);
    }
    return 1;
}

/*
 * Update the clear-all-equipment prompt and empty every occupied slot when accepted.
 *
 * Returns 1 while waiting for input and 0 after the prompt is accepted or dismissed.
 */
static s32 equipment_update_remove_all_prompt(void) {
    u8 unused_buffer[0x40]; /* unreferenced; reserves the target's frame */
    u16 marker_state;
    s32 index;
    s32 unequipped;
    s32 flags;

    g_help_message_text_id = -1;
    gfx_set_otag_index(0x28);
    g_equipment_clear_all_prompt_frame = g_equipment_clear_all_prompt_frame + 1;
    if (g_equipment_clear_all_prompt_frame & 1) {
        for (index = 0; index < 5; index++) {
            g_equipment_remove_all_cursor_position.y = 0x90 + index * 0x10;
            marker_state = 0;
            menu_update_and_draw_animated_cursor(
                &g_equipment_remove_all_cursor_position, &marker_state, g_menu_input_lock_mode);
        }
    }
    flags = g_menu_input_primary_repeat;
    if (flags & PSX_PAD_RIGHT) {
        g_sound_queued_effect_id = SOUND_SFX_PAGE_SWITCH;
        return 0;
    }
    if (flags & PSX_PAD_CIRCLE) {
        unequipped = 0;
        for (index = 0; index < 5; index++) {
            if (g_equipment_unit_data[g_equipment_unit_selected_index]->equipment[index] != 0) {
                unequipped = 1;
                equipment_set_slot_item(g_equipment_unit_selected_index, index, 0);
            }
        }
        equipment_load_selected_unit();
        if (unequipped == 0) {
            g_sound_queued_effect_id = SOUND_SFX_INVALID;
        } else {
            g_sound_queued_effect_id = SOUND_SFX_UNEQUIP;
        }
        return 0;
    }
    return 1;
}

/* Row callback: return the previewed HP bonus of the selected unit; the row is ignored. */
s32 equipment_get_preview_hp_bonus(s32 unused_row) {
    return g_unit_panel_selected_unit_stat_summary.gauges[MENU_UNIT_STATUS_GAUGE_HP].bonus;
}

/* Row callback: return the previewed MP bonus of the selected unit; the row is ignored. */
s32 equipment_get_preview_mp_bonus(s32 unused_row) {
    return g_unit_panel_selected_unit_stat_summary.gauges[MENU_UNIT_STATUS_GAUGE_MP].bonus;
}
