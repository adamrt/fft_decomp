/* Item action state, the type order mode and the equipment slot marker. */

#include "fft/event_equip.h"
#include "fft/gfx.h"
#include "fft/item.h"
#include "fft/main.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/menu_overlay.h"
#include "fft/menu_panel.h"
#include "fft/menu_script.h"
#include "fft/menu_text.h"
#include "fft/sound.h"
#include "fft/text.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Update the secondary item-action menu and return its next screen state. */
s32 equip_menu_update_item_action_state(void) {
    menu_entry_t* menu;

    if (g_equip_item_action_menu_active == 0) {
        if (g_menu_list_entry_count == 0) {
            return MENU_ITEM_LIST_STATE_BROWSE;
        }
        g_item_action_menu.selected_index = 0;
        g_equip_item_action_menu_single_item.selected_index = 0;
        g_menu_help_message_text_id = 0;
        equip_menu_reset_selection_indices();
        g_menu_preview_stats_thread_params.style = 0;
        if (g_item_list_entries[1] != -1) {
            g_equip_item_list_has_multiple_entries = 1;
            menu = &g_item_action_menu;
        } else {
            menu = &g_equip_item_action_menu_single_item;
            g_equip_item_list_has_multiple_entries = 0;
        }
        g_equip_item_action_active_menu = menu;
        g_equip_item_action_menu_active = 1;
    }

    g_menu_help_message_text_id = g_equip_item_action_active_menu->selected_index + 0x102F;
    g_equip_item_action_menu_active
        = menu_start_managed_thread(EQUIP_THREAD_SLOT_POPUP_MENU, g_equip_item_action_active_menu);
    if (g_equip_item_action_menu_active == 0) {
        return MENU_ITEM_LIST_STATE_BROWSE;
    }
    if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
        if (g_item_action_menu.selected_index == 2) {
            g_equip_item_action_menu_active = 0;
            menu_stop_managed_thread(EQUIP_THREAD_SLOT_POPUP_MENU);
            return MENU_ITEM_LIST_STATE_SORT_ORDER;
        }
        g_sound_queued_effect_id = MAIN_SFX_INVALID;
    }
    return MENU_ITEM_LIST_STATE_ACTION;
}

/* Runs the item-type sort sub-menu for the selected equipment slot.
 *
 * A picked item type moves to the front of the slot's type-order list, after
 * which the item list is re-sorted and redrawn. Returns 5 while the sub-menu
 * thread runs and 2 once it closes. */
s32 equip_item_run_type_order_mode(void) {
    s16 names[8];
    s32 i;
    s32 picked;
    s32 first;
    u8 category;

    if (g_equip_type_order_ready == 0) {
        g_text_section_pointers[TEXT_SECTION_COMPOSED_TEXT] = g_equip_item_type_order_names;
        g_equip_type_order_menu_hand_body.selected_index = 0;
        g_equip_type_order_menu_other_slots.selected_index = 0;
        menu_load_sort_key_list(g_menu_selected_slot + 1, g_equip_item_type_order);
        for (i = 0; (names[i] = g_equip_item_type_order[i]) != -1; i++) {
            names[i] += 3;
        }
        text_concatenate_entries(g_text_data, g_equip_item_type_order_names, names, 1);
        g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
        g_equip_type_order_ready = 1;
    }
    if (g_menu_selected_slot == MENU_EQUIPMENT_SLOT_RIGHT_HAND || g_menu_selected_slot == MENU_EQUIPMENT_SLOT_BODY) {
        g_equip_item_type_order_active_menu = &g_equip_type_order_menu_hand_body;
    } else {
        g_equip_item_type_order_active_menu = &g_equip_type_order_menu_other_slots;
    }
    /* i doubles as the help-text id; the target reuses the same register. */
    i = g_equip_item_type_order[g_equip_item_type_order_active_menu->selected_index];
    if (i < 4) {
        i += 0x32;
    }
    if (i == 4) {
        i = 0x37;
    }
    if (i == 5) {
        i = 0x39;
    }
    picked = g_equip_menu_result_rows[2]; /* type-order row picked in the sub-menu, or -1 */
    g_menu_help_message_text_id = i + 0x1000;
    if (picked != -1) {
        first = g_equip_item_type_order[picked];
        for (i = picked - 1; i >= 0; i--) {
            g_equip_item_type_order[i + 1] = g_equip_item_type_order[i];
        }
        g_equip_item_type_order[0] = first;
        equip_menu_reset_selection_indices();
        menu_store_sort_key_list(g_menu_selected_slot + 1, g_equip_item_type_order);
        for (i = 0; (names[i] = g_equip_item_type_order[i]) != -1; i++) {
            names[i] += 3;
        }
        text_concatenate_entries(g_text_data, g_equip_item_type_order_names, names, 1);
        category = g_menu_selected_slot;
        g_equip_menu_text_redraw_flag = 1;
        equip_item_build_filtered_list(g_equip_unit_selected_index, category + 0x81,
            g_menu_selected_slot != MENU_EQUIPMENT_SLOT_ACCESSORY ? category + 1 : ITEM_LIST_FILTER_OTHER,
            g_item_list_entries, ITEM_LIST_MODE_ALL);
        item_store_category_list(g_menu_selected_slot, g_item_list_entries);
        menu_init_scrollable_list_core_variant(
            g_item_list_entries, g_menu_list_selected_index, g_text_section_pointers[TEXT_SECTION_ITEM_NAMES]);
        menu_set_window_scale_step(10);
        item_collect_stat_deltas_variant(g_item_list_entries[g_menu_list_selected_index],
            &g_menu_selected_unit_stat_summary, &g_item_preview_stat_detail, 0);
        g_menu_preview_stats_thread_params.redraw_request = 1;
    }
    if (menu_start_managed_thread(EQUIP_THREAD_SLOT_POPUP_MENU, g_equip_item_type_order_active_menu) == 0) {
        g_equip_type_order_ready = 0;
        return MENU_ITEM_LIST_STATE_ACTION;
    }
    return MENU_ITEM_LIST_STATE_SORT_ORDER;
}

enum {
    EQUIP_SLOT_MARKER_WINDOW_STATE = 0,
    EQUIP_SLOT_MARKER_FIRST_OTAG = 14,
    EQUIP_SLOT_MARKER_SECOND_OTAG = 15,
};

/*
 * Draw the two-part marker for the selected equipment slot.
 *
 * `width` must stay a variable: written as the literal `0x40 - (slot == 0)`,
 * fold turns the subtraction into `slot ? 0x40 : 0x3f` and the compiler emits a
 * branch instead of the target's `li`/`sltiu`/`subu`. Assigning it before the
 * window call also keeps the constant load after the marker address arithmetic.
 *
 * Structurally identical to world_menu_draw_row_window_and_cursor, which draws
 * the same window/marker pair for the world-map menu.
 */
void equip_menu_draw_equipment_slot_marker(s32 unused, s32 slot, s32 override) {
    s16 x = g_equip_menu_slot_marker_x[slot];
    s16 width = 0x40;
    oriented_quad_t* marker;
    oriented_quad_t* secondary;
    s32 secondary_x;

    menu_run_script_with_palette_mode_variant(
        &g_equip_menu_slot_marker_window, EQUIP_SLOT_MARKER_WINDOW_STATE, override);
    marker = &g_equip_menu_slot_marker_rects[slot];
    marker->position.x = x;
    marker->position.y = width - (slot == 0);
    menu_gfx_enqueue_oriented_textured_quad(
        marker, menu_gfx_get_draw_color_variant(), TEXTURE_UV_FLIP_NONE, 0, EQUIP_SLOT_MARKER_FIRST_OTAG);
    secondary = &g_equip_menu_secondary_slot_marker;
    secondary_x = x + 9;
    if (slot == 3) {
        secondary_x = x + 7;
    }
    secondary->position.x = secondary_x;
    g_equip_menu_secondary_slot_marker.position.y = width + 10;
    menu_gfx_enqueue_oriented_textured_quad(secondary, 0, TEXTURE_UV_FLIP_NONE, 0, EQUIP_SLOT_MARKER_SECOND_OTAG);
}
