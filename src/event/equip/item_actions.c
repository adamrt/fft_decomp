/* Item action state, the type order mode and the equipment slot marker. */

#include "fft/equipment.h"
#include "fft/gfx.h"
#include "fft/help.h"
#include "fft/item.h"
#include "fft/item_menu.h"
#include "fft/menu.h"
#include "fft/menu_gfx.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/menu_script.h"
#include "fft/menu_window.h"
#include "fft/sound.h"
#include "fft/text.h"
#include "fft/unit_panel.h"
#include "psx/pad.h"
#include "psx/types.h"

extern s8 g_equipment_type_order_ready;
extern oriented_quad_t g_equipment_slot_marker_rects[];
extern oriented_quad_t g_equipment_secondary_slot_marker;
extern u8 g_equipment_slot_marker_x[];
extern u8 g_equipment_item_list_has_multiple_entries;
extern s8 g_equipment_item_action_menu_active;
static menu_entry_t* g_equipment_item_action_active_menu;
extern menu_entry_t g_equipment_item_action_menu_single_item;
extern s16 g_equipment_text_redraw_flag;
extern s16 g_equipment_item_type_order[];
extern u8 g_equipment_item_type_order_names[];
extern menu_entry_t* g_equipment_item_type_order_active_menu;
extern menu_entry_t g_equipment_type_order_menu_hand_body;
extern menu_entry_t g_equipment_type_order_menu_other_slots;

/* Byte-aligned display streams, including the terminating opcode. */
extern u8 g_equipment_slot_marker_window[121];

/* Run the item list's action menu (a shorter variant when the list holds a single
 * entry). Only row 2, the sort order, does anything here; CIRCLE on any other row plays
 * the invalid sound. Returns MENU_ITEM_LIST_STATE_SORT_ORDER when row 2 is picked,
 * MENU_ITEM_LIST_STATE_BROWSE once the menu closes or when the list is empty, and
 * MENU_ITEM_LIST_STATE_ACTION otherwise. The row test reads g_item_action_menu even when
 * the single-entry menu is the one running. */
s32 equipment_update_item_actions(void) {
    menu_entry_t* menu;

    if (g_equipment_item_action_menu_active == 0) {
        if (g_menu_list_entry_count == 0) {
            return MENU_ITEM_LIST_STATE_BROWSE;
        }
        g_item_action_menu.selected_index = 0;
        g_equipment_item_action_menu_single_item.selected_index = 0;
        g_help_message_text_id = 0;
        equipment_reset_selection();
        g_unit_panel_preview_stats_thread_params.style = 0;
        if (g_item_list_entries[1] != -1) {
            g_equipment_item_list_has_multiple_entries = 1;
            menu = &g_item_action_menu;
        } else {
            menu = &g_equipment_item_action_menu_single_item;
            g_equipment_item_list_has_multiple_entries = 0;
        }
        g_equipment_item_action_active_menu = menu;
        g_equipment_item_action_menu_active = 1;
    }

    /* Each row's help text, from entry 0x2f of HELP_TEXT_SECTION_MENU_COMMANDS (0x1000) on. */
    g_help_message_text_id = g_equipment_item_action_active_menu->selected_index + 0x102F;
    g_equipment_item_action_menu_active
        = menu_managed_thread_start(EQUIPMENT_THREAD_SLOT_POPUP_MENU, g_equipment_item_action_active_menu);
    if (g_equipment_item_action_menu_active == 0) {
        return MENU_ITEM_LIST_STATE_BROWSE;
    }
    if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
        if (g_item_action_menu.selected_index == 2) {
            g_equipment_item_action_menu_active = 0;
            menu_managed_thread_stop(EQUIPMENT_THREAD_SLOT_POPUP_MENU);
            return MENU_ITEM_LIST_STATE_SORT_ORDER;
        }
        g_sound_queued_effect_id = SOUND_SFX_INVALID;
    }
    return MENU_ITEM_LIST_STATE_ACTION;
}

/* Runs the item-type sort sub-menu for the selected equipment slot.
 *
 * A picked item type moves to the front of the slot's type-order list, after
 * which the item list is re-sorted and redrawn. The rows show text entry type + 3
 * of the overlay's text; the right-hand and body slots use one menu descriptor and
 * the other slots another. Returns
 * MENU_ITEM_LIST_STATE_SORT_ORDER while the sub-menu thread runs and
 * MENU_ITEM_LIST_STATE_ACTION once it closes. */
s32 equipment_type_order_update(void) {
    s16 names[8];
    s32 i;
    s32 picked;
    s32 first;
    u8 category;

    if (g_equipment_type_order_ready == 0) {
        g_text_section_pointers[TEXT_SECTION_COMPOSED_TEXT] = g_equipment_item_type_order_names;
        g_equipment_type_order_menu_hand_body.selected_index = 0;
        g_equipment_type_order_menu_other_slots.selected_index = 0;
        item_load_type_order(g_menu_selected_slot + 1, g_equipment_item_type_order);
        for (i = 0; (names[i] = g_equipment_item_type_order[i]) != -1; i++) {
            names[i] += 3;
        }
        text_concatenate_entries(g_text_data, g_equipment_item_type_order_names, names, 1);
        g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
        g_equipment_type_order_ready = 1;
    }
    if (g_menu_selected_slot == MENU_EQUIPMENT_SLOT_RIGHT_HAND || g_menu_selected_slot == MENU_EQUIPMENT_SLOT_BODY) {
        g_equipment_item_type_order_active_menu = &g_equipment_type_order_menu_hand_body;
    } else {
        g_equipment_item_type_order_active_menu = &g_equipment_type_order_menu_other_slots;
    }
    /* i doubles as the help-text id (in HELP_TEXT_SECTION_MENU_COMMANDS: types 0-3 at 0x32 on,
     * 4 at 0x37, 5 at 0x39); the target reuses the same register. */
    i = g_equipment_item_type_order[g_equipment_item_type_order_active_menu->selected_index];
    if (i < 4) {
        i += 0x32;
    }
    if (i == 4) {
        i = 0x37;
    }
    if (i == 5) {
        i = 0x39;
    }
    picked = g_equipment_result_rows[2]; /* type-order row picked in the sub-menu, or -1 */
    g_help_message_text_id = i + HELP_TEXT_ID_MENU_COMMAND_BASE;
    if (picked != -1) {
        first = g_equipment_item_type_order[picked];
        for (i = picked - 1; i >= 0; i--) {
            g_equipment_item_type_order[i + 1] = g_equipment_item_type_order[i];
        }
        g_equipment_item_type_order[0] = first;
        equipment_reset_selection();
        item_store_type_order(g_menu_selected_slot + 1, g_equipment_item_type_order);
        for (i = 0; (names[i] = g_equipment_item_type_order[i]) != -1; i++) {
            names[i] += 3;
        }
        text_concatenate_entries(g_text_data, g_equipment_item_type_order_names, names, 1);
        category = g_menu_selected_slot;
        g_equipment_text_redraw_flag = 1;
        equipment_build_item_list(g_equipment_unit_selected_index, category + 0x81,
            g_menu_selected_slot != ITEM_MENU_TAB_OTHER ? category + 1 : ITEM_LIST_FILTER_OTHER, g_item_list_entries,
            ITEM_LIST_MODE_ALL);
        item_store_category_list(g_menu_selected_slot, g_item_list_entries);
        menu_scrollable_list_core_init(
            g_item_list_entries, g_menu_list_selected_index, g_text_section_pointers[TEXT_SECTION_ITEM_NAMES]);
        menu_set_window_scale_step(10);
        item_stat_preview_build(g_item_list_entries[g_menu_list_selected_index],
            &g_unit_panel_selected_unit_stat_summary, &g_item_preview_stat_detail, 0);
        g_unit_panel_preview_stats_thread_params.redraw_request = 1;
    }
    if (menu_managed_thread_start(EQUIPMENT_THREAD_SLOT_POPUP_MENU, g_equipment_item_type_order_active_menu) == 0) {
        g_equipment_type_order_ready = 0;
        return MENU_ITEM_LIST_STATE_ACTION;
    }
    return MENU_ITEM_LIST_STATE_SORT_ORDER;
}

enum {
    EQUIPMENT_SLOT_MARKER_WINDOW_STATE = 0,
    EQUIPMENT_SLOT_MARKER_FIRST_OTAG = 14,
    EQUIPMENT_SLOT_MARKER_SECOND_OTAG = 15
};

/*
 * Draw the two-part marker for the selected equipment slot.
 *
 * `base_y` must stay a variable: written as the literal `0x40 - (slot == 0)`,
 * fold turns the subtraction into `slot ? 0x40 : 0x3f` and the compiler emits a
 * branch instead of the target's `li`/`sltiu`/`subu`. Assigning it before the
 * window call also keeps the constant load after the marker address arithmetic.
 *
 * Structurally identical to screen_item_tabs_draw, which draws
 * the same window/marker pair for the world-map menu.
 */
void equipment_draw_slot_marker(s32 unused, s32 slot, s32 override) {
    s16 x = g_equipment_slot_marker_x[slot];
    s16 base_y = 0x40;
    oriented_quad_t* marker;
    oriented_quad_t* secondary;
    s32 secondary_x;

    menu_run_script_with_palette_mode(g_equipment_slot_marker_window, EQUIPMENT_SLOT_MARKER_WINDOW_STATE, override);
    marker = &g_equipment_slot_marker_rects[slot];
    marker->position.x = x;
    marker->position.y = base_y - (slot == 0);
    menu_gfx_enqueue_oriented_textured_quad(
        marker, menu_gfx_get_draw_color(), TEXTURE_UV_FLIP_NONE, 0, EQUIPMENT_SLOT_MARKER_FIRST_OTAG);
    secondary = &g_equipment_secondary_slot_marker;
    secondary_x = x + 9;
    if (slot == 3) {
        secondary_x = x + 7;
    }
    secondary->position.x = secondary_x;
    secondary->position.y = base_y + 10;
    menu_gfx_enqueue_oriented_textured_quad(secondary, 0, TEXTURE_UV_FLIP_NONE, 0, EQUIPMENT_SLOT_MARKER_SECOND_OTAG);
}
