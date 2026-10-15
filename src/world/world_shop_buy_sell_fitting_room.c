/* Buy, sell, equip candidate and fitting room steps. */

#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/world.h"
#include "psx/libc.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Structural twin of world_shop_run_soldier_office_fee_menu_script (0x80138ea4): install a menu value
 * callback, run the menu script, then restore the previous callback. */
void world_shop_run_obtain_gil_menu(void) {
    menu_callback_t* callback_slot;
    s32 (*previous_callback)(s32);

    callback_slot = g_world_menu_script_callbacks;
    previous_callback = callback_slot->value;
    callback_slot->value = world_shop_adjust_gil;
    menu_run_script_with_palette_mode_variant(&g_world_shop_obtain_gil_menu_script, 0, 0);
    callback_slot->value = previous_callback;
}

/* Returns the owned-and-equipped count for the selected item. */
s32 world_item_count_selected_owned_and_equipped(s32 unused_row) {
    s32 result;

    if (g_menu_list_entry_count != 0) {
        result = item_get_total_count_variant(g_item_list_entries[g_menu_list_selected_index] & WORLD_ITEM_ID_MASK);
    } else {
        result = 0;
    }
    return result;
}

/* Returns the party-equipped count for the selected item. */
s32 world_item_count_selected_equipped_by_party(s32 unused_row) {
    s32 result;

    if (g_menu_list_entry_count != 0) {
        result = item_get_equipped_count_variant(g_item_list_entries[g_menu_list_selected_index] & WORLD_ITEM_ID_MASK);
    } else {
        result = 0;
    }
    return result;
}

/* Full-price twin of world_menu_get_item_half_price_display_value: the menu
 * item's price with bit 30 copied from its 0x4000 entry flag. */
s32 world_menu_get_item_price_display_value(s32 entry_index) {
    s32 display_value = world_item_get_price(g_item_list_entries[entry_index]);

    if ((g_item_list_entries[entry_index] & WORLD_ITEM_LIST_ENTRY_UNEQUIPPABLE) != 0)
        display_value |= MENU_NUMBER_VALUE_DISABLED;
    return display_value;
}

/* Switch the active item category and restore its saved list cursor. */
void world_menu_switch_item_category(s32 input) {
    s16 x;
    s16 y;
    s32 category;
    s32 cursor_slot;
    s16 item;
    s32 description;

    category = menu_update_horizontal_selection_and_mark_change(
        5, WORLD_MENU_CURSOR_SHOP_ITEM_TAB, input, MAIN_SFX_PAGE_SWITCH);
    cursor_slot = g_world_shop_item_category;
    if (cursor_slot != category) {
        if (g_world_shop_menu_step != WORLD_SHOP_STEP_BUY_LIST
            && g_world_shop_menu_step != WORLD_SHOP_STEP_FUR_BUY_LIST) {
            cursor_slot += 5;
        }
        menu_set_selection_record(
            cursor_slot, g_menu_list_selected_index, g_menu_scroll_base_index, g_item_list_entries);
        g_world_shop_item_category = category;
        if (g_world_shop_menu_step == WORLD_SHOP_STEP_BUY_LIST
            || g_world_shop_menu_step == WORLD_SHOP_STEP_FUR_BUY_LIST) {
            world_shop_build_item_list(-1, g_world_menu_context_id,
                category != 4 ? (category + 1) : ITEM_LIST_FILTER_OTHER, g_item_list_entries, ITEM_LIST_MODE_ALL);
            cursor_slot = g_world_shop_item_category;
        } else {
            world_item_build_category_list(-1, g_world_shop_item_category + 1,
                category != 4 ? category + 1 : ITEM_LIST_FILTER_OTHER, g_item_list_entries, ITEM_LIST_MODE_ALL);
            cursor_slot = g_world_shop_item_category + 5;
        }
        menu_get_selection_record_variant(cursor_slot, &x, &y, g_item_list_entries);
        menu_init_scrollable_list(g_item_list_entries, x, y, g_text_section_pointers[TEXT_SECTION_ITEM_NAMES]);
        menu_set_window_scale_step(10);
    }
    world_menu_draw_row_window_and_cursor(1, g_world_shop_item_category, g_menu_input_lock_mode);
    item = g_item_list_entries[g_menu_list_selected_index];
    if (item != -1) {
        description = (item & WORLD_ITEM_ID_MASK) + 0x6800;
    } else {
        description = 0;
    }
    g_menu_help_message_text_id = description;
}

/* Handle item selection and purchase-menu transitions.
 */
void world_shop_handle_item_purchase_menu(void) {
    s16 x;
    s16 y;
    s32 input;
    s32 disabled;
    s8 result;

    if (g_world_shop_purchase_list_initialized == 0) {
        if (thread_is_running(THREAD_SCREEN_SLOT_MESSAGE) == 0) {
            thread_start(THREAD_SCREEN_SLOT_MESSAGE, world_text_message_box_thread);
        }
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF804, 0);
        g_world_shop_purchase_list_initialized = 1;
        world_shop_build_item_list(-1, g_world_menu_context_id,
            g_world_shop_item_category != 4 ? g_world_shop_item_category + 1 : ITEM_LIST_FILTER_OTHER,
            g_item_list_entries, ITEM_LIST_MODE_ALL);
        menu_get_selection_record_variant(g_world_shop_item_category, &x, &y, g_item_list_entries);
        menu_init_scrollable_list(g_item_list_entries, x, y, g_text_section_pointers[TEXT_SECTION_ITEM_NAMES]);
        g_world_menu_script_callbacks[0].value = menu_get_entry_item_equipped_count;
        g_world_menu_script_callbacks[1].value = menu_get_entry_item_count;
        g_world_menu_script_callbacks[2].icon = menu_build_item_type_icon_source;
        g_world_menu_script_callbacks[3].icon = menu_get_entry_item_icon_source;
        g_world_menu_script_callbacks[4].value = world_menu_get_item_price_display_value;
        g_world_menu_script_callbacks[5].value = world_item_count_selected_equipped_by_party;
        g_world_menu_script_callbacks[6].value = world_item_count_selected_owned_and_equipped;
        g_world_shop_purchase_equip_check_pending = 0;
    }
    menu_set_vsync_mode_and_event_speed(2);
    if (g_world_shop_purchase_equip_check_pending != 0) {
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
        result = thread_is_running(THREAD_SCREEN_SLOT_MESSAGE);
        g_world_shop_purchase_equip_check_pending = result;
        if (result == 0) {
            g_world_shop_purchase_list_initialized = 0;
            menu_set_selection_record(
                g_world_shop_item_category, g_menu_list_selected_index, g_menu_scroll_base_index, g_item_list_entries);
            g_world_shop_menu_step = WORLD_SHOP_STEP_BUY_EQUIP;
        }
        menu_input_clear_state();
    } else {
        input = g_menu_input_primary_repeat;
        if (input & PSX_PAD_CROSS) {
            g_sound_queued_effect_id = MAIN_SFX_CANCEL;
            g_world_shop_purchase_list_initialized = 0;
            g_world_shop_menu_step = WORLD_SHOP_STEP_MAIN_MENU;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF802, 0);
        } else if ((input & PSX_PAD_TRIANGLE) && g_menu_list_entry_count != 0 && g_world_shop_item_category != 4) {
            g_world_shop_purchase_equip_check_pending = 1;
        } else if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
            if (g_menu_list_entry_count == 0) {
                g_sound_queued_effect_id = MAIN_SFX_INVALID;
            } else {
                g_world_shop_menu_step = WORLD_SHOP_STEP_BUY_CONFIRM;
                g_world_shop_quantity_menu_data[1].selected_index = 0;
                g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
            }
        }
    }
    disabled = 0;
    world_menu_switch_item_category(g_menu_input_secondary_repeat);
    menu_run_script_with_palette_mode_variant(
        &g_world_shop_purchase_menu_script, g_menu_input_primary_repeat, g_menu_input_lock_mode);
    if (world_map_is_busy() != 0 || g_menu_list_entry_count == 0 || g_world_shop_item_category == 4) {
        disabled = 1;
    }
    world_menu_draw_pressable_button(MENU_PRESSABLE_BUTTON_SHOP_LIST_TRIANGLE, 0xBA, 0x4C,
        g_menu_input_primary_repeat & PSX_PAD_TRIANGLE, disabled, MAIN_SFX_CONFIRM);
    world_shop_run_obtain_gil_menu();
}

/*
 * Shop buy step: runs the purchase-quantity prompt for the highlighted item
 * and completes the purchase.
 *
 * On "yes" the gil is deducted (price times quantity), the items are added to
 * the inventory and message 0xF811 is shown; on "no" message 0xF80A. That
 * message, or the prompt's refusal (stack full or not affordable), stays
 * until any face button or d-pad press, which returns to the buy list. The
 * list, the Triangle button and the gil display are drawn every frame.
 */
void world_menu_confirm_item_purchase(void) {
    s32 result = 1;

    if (g_world_shop_purchase_message_wait != 0) {
        if (g_menu_input_primary_repeat
            & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE | PSX_PAD_DPAD_MASK)) {
            g_world_shop_purchase_message_wait = 0;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF805, 0);
            g_world_menu_prompt_active = 0;
            g_world_shop_menu_step = WORLD_SHOP_STEP_BUY_LIST;
        }
    } else {
        result = world_shop_run_item_purchase_quantity_menu();
    }
    if (result == 0) {
        if (g_world_menu_selection_results[WORLD_MENU_SELECTION_PROMPT_RESULT] == 0) {
            world_shop_adjust_gil(-world_item_get_price(g_item_list_entries[g_menu_list_selected_index])
                * g_world_menu_selection_results[WORLD_MENU_SELECTION_SHOP_QUANTITY]);
            item_adjust_inventory_count(g_item_list_entries[g_menu_list_selected_index],
                g_world_menu_selection_results[WORLD_MENU_SELECTION_SHOP_QUANTITY]);
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF811, 0);
            item_rebuild_category_list(g_world_shop_item_category);
        } else {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF80A, 0);
        }
    }
    if (result <= 0) {
        g_world_shop_purchase_message_wait = 1;
    }
    world_menu_switch_item_category(0);
    menu_run_script_with_palette_mode_variant(&g_world_shop_purchase_menu_script, 0, g_menu_input_lock_mode);
    world_menu_draw_pressable_button(MENU_PRESSABLE_BUTTON_SHOP_LIST_TRIANGLE, 0xBA, 0x4C, 0, 1, MAIN_SFX_CONFIRM);
    world_shop_run_obtain_gil_menu();
}

/* Runs the shop purchase-quantity menu for the highlighted item.
 *
 * On entry, caps the selectable quantity at min(gil / price, 99 - owned) and
 * returns -2 when the stack is already full or -1 when no unit is
 * affordable. Afterwards it returns the quantity menu thread state, updating
 * the total-price prompt while the confirmation sub-thread is running. */
s32 world_shop_run_item_purchase_quantity_menu(void) {
    s32 owned;
    s32 count;
    s32 limit;

    if (g_world_shop_quantity_menu_running == 0) {
        g_world_shop_purchase_total_shown = 0;
        g_world_menu_prompt_active = 1;
        g_world_shop_purchase_item = g_item_list_entries[g_menu_list_selected_index];
        owned = item_get_total_count_variant(g_world_shop_purchase_item);
        if (owned >= 99) {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF807, 0);
            return WORLD_SHOP_QUANTITY_HOLDING_MAX;
        }
        count = world_shop_adjust_gil(0);
        g_world_shop_purchase_unit_price = world_item_get_price(g_world_shop_purchase_item);
        count /= g_world_shop_purchase_unit_price;
        limit = 99 - owned;
        if (limit < count) {
            count = limit;
        }
        if (count == 0) {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF806, 0);
            return WORLD_SHOP_QUANTITY_CANNOT_AFFORD;
        }
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF808, 0);
        g_world_shop_quantity_limit = count;
        g_world_shop_purchase_total_shown = 0;
        g_sound_suppressed = 1;
    }
    g_world_shop_quantity_menu_running
        = menu_start_managed_thread(WORLD_THREAD_SLOT_UPPER_MENU, g_world_shop_quantity_menu_data);
    if (thread_is_running(WORLD_THREAD_SLOT_UPPER_MENU_CHILD) != 0 && g_world_shop_purchase_total_shown == 0) {
        g_text_substitution_values[0]
            = g_world_menu_selection_results[WORLD_MENU_SELECTION_SHOP_QUANTITY] * g_world_shop_purchase_unit_price;
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF809, 0);
        g_world_shop_purchase_total_shown = 1;
        g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
    }
    if (thread_is_running(WORLD_THREAD_SLOT_UPPER_MENU_CHILD) == 0 && g_world_shop_purchase_total_shown != 0) {
        g_world_shop_purchase_total_shown = 0;
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF808, 0);
    }
    if (g_world_shop_quantity_menu_running == 0) {
        if (g_world_menu_selection_results[WORLD_MENU_SELECTION_PROMPT_RESULT] == 0) {
            g_sound_queued_effect_id = MAIN_SFX_SHOP_TRANSACTION;
        }
        if (g_world_menu_selection_results[WORLD_MENU_SELECTION_PROMPT_RESULT] == 1) {
            g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
        }
        g_sound_suppressed = 0;
    }
    return g_world_shop_quantity_menu_running;
}

/* Equip-menu marker for the selected item: 8 when the unit already wears it
 * or cannot equip it, -1 when it is a fresh equippable choice. */
s32 world_item_get_equip_candidate_marker(s32 unit_index) {
    world_unit_t* unit = g_world_unit_pointers[unit_index];
    s32 i;

    for (i = 0; i < WORLD_EQUIPMENT_SLOT_COUNT; i++) {
        if (unit->equipment[i] == g_world_shop_equip_candidate_item) {
            return MENU_UNIT_READOUT_EQUIP_LABEL;
        }
    }
    if (world_unit_can_equip_item_id(unit_index, g_world_shop_equip_candidate_item) != 1) {
        return MENU_UNIT_READOUT_EQUIP_LABEL;
    }
    return -1;
}

/* Menu callback: sprite brightness for a unit on the shop's "who can equip"
 * view: 0x80 when the unit can equip the candidate item, 0x40 when not. */
s32 world_item_get_equip_candidate_brightness(s32 unit_index) {
    return world_unit_can_equip_item_id(unit_index, g_world_shop_equip_candidate_item) == 1 ? 0x80 : 0x40;
}

/*
 * Run one frame of the shop's "who can equip" unit browser for the item
 * under the list cursor.
 *
 * Moving onto a unit that can equip the item stages it in the temporary
 * formation preview record and shows the stat preview; slots
 * already holding the item are flagged in g_world_selected_unit_stat_detail.state.equipment. Cancel returns to shop
 * step 1.
 */
void world_shop_run_equip_candidate_step(void) {
    s32 unused[16];
    s32 input;
    u8 equipped;
    s32 i;
    s32 slot;
    s32 text_id;

    input = g_menu_input_secondary_repeat;
    if (g_world_shop_equip_candidate_initialized == 0) {
        menu_set_vsync_mode_and_event_speed(0);
        g_world_formation_unit_browse_enabled = 0;
        g_world_shop_background_visible = 0;
        g_world_shop_equip_candidate_item = g_item_list_entries[g_menu_list_selected_index];
        world_formation_init_cursor_and_scroll_state(0);
        g_world_shop_equip_candidate_preview_visible = 0;
        g_world_shop_equip_candidate_initialized = 1;
    }
    g_world_shop_equip_candidate_saved_browse = g_world_formation_unit_browse_enabled;
    world_formation_update_and_draw_unit_grid(0, g_world_formation_unit_browse_enabled == 0, 1, input,
        g_menu_input_lock_mode, world_item_get_equip_candidate_marker, world_item_get_equip_candidate_brightness);
    if (g_world_shop_equip_candidate_saved_browse == 0) {
        if (g_world_formation_unit_browse_enabled != 0) {
            g_world_shop_equip_candidate_last_unit = -1;
            return;
        }
    } else if (g_world_formation_unit_browse_enabled == 0) {
        g_menu_status_display_thread_params.redraw_request = 1;
    }
    if (g_world_formation_unit_banner_enabled != 0 && g_world_formation_unit_browse_enabled == 0) {
        text_id = MENU_GRID_HELP_BANNER_TOP;
        if (g_world_formation_unit_banner_at_bottom != 0) {
            text_id = MENU_GRID_HELP_BANNER_BOTTOM;
        }
        g_menu_help_message_text_id = text_id;
    } else {
        g_menu_help_message_text_id = 0;
    }
    if (g_world_formation_unit_browse_enabled != 0) {
        if (g_unit_selected_index != g_world_shop_equip_candidate_last_unit) {
            world_formation_stage_selected_unit();
            i = 0;
            equipped = 0;
            g_world_shop_equip_candidate_last_unit = g_unit_selected_index;
            for (; i < WORLD_EQUIPMENT_SLOT_COUNT; i++) {
                if (g_world_unit_pointers[g_unit_selected_index]->equipment[i] == g_world_shop_equip_candidate_item) {
                    equipped = 1;
                    g_world_selected_unit_stat_detail.state.equipment[i] |= 0x8000;
                }
            }
            if (equipped != 0
                || world_unit_can_equip_item_id(g_unit_selected_index, g_world_shop_equip_candidate_item) != 1) {
                g_world_shop_equip_candidate_preview_visible = 0;
            } else {
                slot = world_item_select_equipment_slot(
                    WORLD_FORMATION_PREVIEW_UNIT_INDEX, g_world_shop_equip_candidate_item);
                world_formation_equip_item_to_unit_slot(
                    WORLD_FORMATION_PREVIEW_UNIT_INDEX, slot, g_world_shop_equip_candidate_item);
                bcopy(g_world_unit_pointers[WORLD_FORMATION_PREVIEW_UNIT_INDEX]->equipment,
                    g_item_preview_stat_detail.state.equipment, 10);
                item_calculate_equipment_swap_stat_delta(&g_item_preview_stat_detail,
                    &g_menu_selected_unit_stat_summary, g_world_unit_pointers[g_unit_selected_index]->equipment,
                    g_world_unit_pointers[WORLD_FORMATION_PREVIEW_UNIT_INDEX]->equipment);
                g_item_preview_stat_detail.state.two_hands = unit_is_two_handing_weapon(
                    g_item_preview_stat_detail.state.equipment, world_ability_has_two_hands(g_unit_selected_index));
                g_world_shop_equip_candidate_preview_visible = 1;
                g_item_preview_stat_detail.state.equipment[slot]
                    = g_world_shop_equip_candidate_item | WORLD_EQUIPMENT_SHOP_ITEM;
            }
        }
    } else {
        g_world_shop_equip_candidate_preview_visible = 0;
        if (g_world_shop_equip_candidate_saved_browse == 0 && (input & PSX_PAD_CROSS)) {
            g_sound_queued_effect_id = MAIN_SFX_CANCEL;
            g_world_formation_unit_banner_enabled = 0;
            g_world_shop_menu_step = WORLD_SHOP_STEP_BUY_LIST;
            menu_set_vsync_mode_and_event_speed(2);
            g_world_shop_background_visible = 1;
            g_world_shop_equip_candidate_initialized = 0;
        }
    }
    world_menu_toggle_preview_stats_window(g_world_shop_equip_candidate_preview_visible);
    menu_toggle_stat_preview_panel_thread(g_world_shop_equip_candidate_preview_visible);
}

/* Menu callback: half price of the highlighted item, or 0 for an empty list.
 * The row argument is not read. */
s32 world_item_get_selected_half_price(s32 unused_row) {
    s32 result;

    if (g_menu_list_entry_count == 0) {
        result = 0;
    } else {
        result = world_item_get_half_price(g_item_list_entries[g_menu_list_selected_index]);
    }
    return result;
}

/* Item-list shop menu step: on first entry prompts 0xF80B (0xF80C when the
 * player already holds items), rebuilds the item list and installs the seven
 * column callbacks, then handles cancel (0x40) and confirm (0x20).
 */
void world_shop_run_item_sell_list_step(void) {
    if (g_world_shop_sell_list_initialized == 0) {
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, world_menu_has_items(0) != 0 ? 0xF80C : 0xF80B, 0);
        g_world_shop_item_category = menu_init_cursor_position(WORLD_MENU_CURSOR_SHOP_ITEM_TAB, 0);
        world_item_build_category_list(-1, g_world_shop_item_category + 1,
            g_world_shop_item_category != 4 ? g_world_shop_item_category + 1 : ITEM_LIST_FILTER_OTHER,
            g_item_list_entries, ITEM_LIST_MODE_ALL);
        menu_init_scrollable_list_core_variant(
            g_item_list_entries, g_menu_list_selected_index, g_text_section_pointers[TEXT_SECTION_ITEM_NAMES]);
        g_world_menu_script_callbacks[0].value = menu_get_entry_item_equipped_count;
        g_world_menu_script_callbacks[1].value = menu_get_entry_item_count;
        g_world_menu_script_callbacks[2].icon = menu_build_item_type_icon_source;
        g_world_menu_script_callbacks[3].icon = menu_get_entry_item_icon_source;
        g_world_menu_script_callbacks[4].value = world_item_get_selected_half_price;
        g_world_menu_script_callbacks[5].value = menu_get_entry_item_equipped_count;
        g_world_menu_script_callbacks[6].value = menu_get_entry_item_count;
        g_world_shop_sell_list_initialized = 1;
    }
    menu_set_vsync_mode_and_event_speed(2);
    if (g_menu_input_primary_repeat & PSX_PAD_CROSS) {
        g_world_shop_sell_list_initialized = 0;
        g_sound_queued_effect_id = MAIN_SFX_CANCEL;
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF802, 0);
        g_world_shop_menu_step = WORLD_SHOP_STEP_MAIN_MENU;
    } else if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
        if (g_menu_list_entry_count == 0) {
            g_sound_queued_effect_id = MAIN_SFX_INVALID;
        } else {
            g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
            g_world_shop_quantity_menu_data[1].selected_index = 0;
            g_world_shop_menu_step = WORLD_SHOP_STEP_SELL_CONFIRM;
        }
    }
    world_menu_switch_item_category(g_menu_input_secondary_repeat);
    menu_run_script_with_palette_mode_variant(
        &g_world_shop_sell_list_menu_script, g_menu_input_primary_repeat, g_menu_input_lock_mode);
    world_shop_run_obtain_gil_menu();
}

/* Item sell confirmation step: pays half price times the quantity, removes the
 * items, prompts 0xF811 and rebuilds the list (0xF80A when nothing to sell).
 */
void world_shop_run_item_sell_confirm_step(void) {
    s32 result = 1;

    if (g_world_shop_sell_message_wait != 0) {
        if (g_menu_input_primary_repeat
            & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE | PSX_PAD_DPAD_MASK)) {
            g_world_shop_sell_message_wait = 0;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF80D, 0);
            g_world_shop_menu_step = WORLD_SHOP_STEP_SELL_LIST;
            g_world_menu_prompt_active = 0;
        }
    } else {
        result = world_shop_run_item_sell_menu();
    }
    if (result == 0) {
        if (g_world_menu_selection_results[WORLD_MENU_SELECTION_PROMPT_RESULT] == 0) {
            world_shop_adjust_gil(world_item_get_half_price(g_item_list_entries[g_menu_list_selected_index])
                * g_world_menu_selection_results[WORLD_MENU_SELECTION_SHOP_QUANTITY]);
            item_adjust_inventory_count(g_item_list_entries[g_menu_list_selected_index],
                -g_world_menu_selection_results[WORLD_MENU_SELECTION_SHOP_QUANTITY]);
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF811, 0);
            item_rebuild_category_list(g_world_shop_item_category);
            world_item_build_category_list(0, g_world_shop_item_category + 1,
                g_world_shop_item_category != 4 ? g_world_shop_item_category + 1 : ITEM_LIST_FILTER_OTHER,
                g_item_list_entries, ITEM_LIST_MODE_ALL);
            world_menu_init_and_load_scrollable_list(g_item_list_entries, g_menu_list_selected_index,
                g_text_section_pointers[TEXT_SECTION_ITEM_NAMES], &g_world_shop_sell_list_menu_script);
            menu_set_window_scale_step(10);
        } else {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF80A, 0);
        }
    }
    if (result <= 0) {
        g_world_shop_sell_message_wait = 1;
    }
    world_menu_switch_item_category(0);
    menu_run_script_with_palette_mode_variant(
        &g_world_shop_sell_list_menu_script, 0, thread_is_running(THREAD_SCREEN_SLOT_TEXT));
    world_shop_run_obtain_gil_menu();
}

/* Shop: the quantity prompt of a sale, capped by the inventory count.
 *
 * The first call returns -1 with message 0xF80E when none of the highlighted
 * item is in the inventory. Otherwise it returns the quantity menu's thread
 * state, showing the total (half price times quantity, message 0xF810) while
 * the confirmation child runs and the plain prompt (0xF80F) otherwise. The
 * final frame queues the transaction sound for "yes" and the confirm sound
 * for "no". */
s32 world_shop_run_item_sell_menu(void) {
    s32 count;

    if (g_world_shop_sell_result == 0) {
        g_world_shop_sell_confirm_shown = 0;
        g_world_shop_sell_item = g_item_list_entries[g_menu_list_selected_index];
        g_world_menu_prompt_active = 1;
        count = item_adjust_inventory_count(g_world_shop_sell_item, 0);
        if (count == 0) {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF80E, 0);
            return -1;
        }
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF80F, 0);
        g_world_shop_quantity_limit = count;
        g_world_shop_sell_confirm_shown = 0;
        g_sound_suppressed = 1;
    }
    g_world_shop_sell_result = menu_start_managed_thread(WORLD_THREAD_SLOT_UPPER_MENU, g_world_shop_quantity_menu_data);
    if (thread_is_running(WORLD_THREAD_SLOT_UPPER_MENU_CHILD) != 0 && g_world_shop_sell_confirm_shown == 0) {
        g_text_substitution_values[0] = world_item_get_half_price(g_world_shop_sell_item)
            * g_world_menu_selection_results[WORLD_MENU_SELECTION_SHOP_QUANTITY];
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF810, 0);
        g_world_shop_sell_confirm_shown = 1;
        g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
    }
    if (thread_is_running(WORLD_THREAD_SLOT_UPPER_MENU_CHILD) == 0 && g_world_shop_sell_confirm_shown != 0) {
        g_world_shop_sell_confirm_shown = 0;
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF80F, 0);
    }
    if (g_world_shop_sell_result == 0) {
        if (g_world_menu_selection_results[WORLD_MENU_SELECTION_PROMPT_RESULT] == 0) {
            g_sound_queued_effect_id = MAIN_SFX_SHOP_TRANSACTION;
        }
        if (g_world_menu_selection_results[WORLD_MENU_SELECTION_PROMPT_RESULT] == 1) {
            g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
        }
        g_sound_suppressed = 0;
    }
    return g_world_shop_sell_result;
}

/* Menu callback: add `delta` to the fitting room's running cost and return
 * the new total. */
s32 world_shop_add_fitting_room_cost(s32 delta) {
    return g_world_shop_fitting_room_cost = delta + g_world_shop_fitting_room_cost;
}

/* Menu callback: no marker (-1) for any unit. */
s32 world_shop_fitting_room_get_unit_marker(s32 unused_unit_index) {
    return -1;
}

/* Fitting-room menu step: on entry clear the trial equipment and its cost;
 * then either browse the formation (confirm opens the fitting menu, cancel
 * returns to shop step 0xC) or run the fitting menu thread and dispatch its
 * selection to the next shop step. */
void world_shop_run_fitting_room_step(void) {
    s32 busy;

    if (g_world_shop_fitting_room_initialized == 0) {
        if (g_menu_input_newly_pressed == 0) {
            world_shop_run_obtain_gil_menu();
            return;
        }
        world_formation_init_cursor_and_scroll_state(0);
        memset(g_world_shop_fitting_room_items, 0, sizeof(g_world_shop_fitting_room_items));
        world_shop_add_fitting_room_cost(-world_shop_add_fitting_room_cost(0));
        g_world_shop_fitting_room_unit_chosen = 0;
        g_world_shop_cost_window_visible = 0;
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
        g_world_shop_fitting_room_initialized = 1;
        return;
    }
    if (g_world_shop_fitting_room_unit_chosen == 0) {
        g_world_shop_background_visible = 0;
        if (g_world_formation_unit_banner_enabled != 0) {
            g_menu_help_message_text_id = (g_world_formation_unit_banner_at_bottom != 0) ? MENU_GRID_HELP_BANNER_BOTTOM
                                                                                         : MENU_GRID_HELP_BANNER_TOP;
        } else {
            g_menu_help_message_text_id = 0;
        }
        busy = world_input_get_lockout_timer();
        busy += world_formation_is_unit_list_scroll_needed();
        if ((g_menu_input_primary_repeat & PSX_PAD_CROSS) && busy == 0) {
            g_sound_queued_effect_id = MAIN_SFX_CANCEL;
            g_world_formation_unit_banner_enabled = 0;
            g_world_shop_menu_step = WORLD_SHOP_STEP_FITTING_ROOM_CHECKOUT;
        } else if ((g_menu_input_primary_repeat & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE)) && busy == 0) {
            g_world_formation_unit_cycle_mode = 2;
            g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
            g_world_shop_fitting_room_unit_chosen = 1;
            g_world_formation_unit_browse_enabled = 1;
            g_world_formation_unit_banner_at_bottom = 0;
            g_world_shop_cost_window_visible = 1;
            g_world_shop_fitting_room_menu_thread_data[0].selected_index = 0;
            g_world_shop_fitting_room_menu_thread_data[1].selected_index = 0;
            menu_clear_selection_records();
        } else {
            world_formation_update_and_draw_unit_grid(1, 1, 0, g_menu_input_secondary_repeat, g_menu_input_lock_mode,
                world_shop_fitting_room_get_unit_marker, 0);
        }
    } else {
        g_world_shop_background_visible = 1;
        g_menu_help_message_text_id = 0;
        if (menu_start_managed_thread(THREAD_SCREEN_SLOT_MENU, g_world_shop_fitting_room_menu_thread_data) == 0) {
            if (g_world_menu_selection_results[1] == 0) {
                g_world_shop_menu_step = WORLD_SHOP_STEP_FITTING_ROOM_EQUIP;
            } else if (g_world_menu_selection_results[1] == 1) {
                g_world_shop_menu_step = WORLD_SHOP_STEP_FITTING_ROOM_BEST_FIT;
            } else if (g_world_menu_selection_results[1] == 2) {
                g_world_shop_menu_step = WORLD_SHOP_STEP_FITTING_ROOM_REMOVE;
            } else if (g_world_menu_selection_results[1] == 3) {
                if (g_world_menu_selection_results[2] == 0) {
                    g_world_shop_menu_step = WORLD_SHOP_STEP_FITTING_ROOM_SELL_EQUIPPED;
                } else {
                    g_world_shop_menu_step = WORLD_SHOP_STEP_FITTING_ROOM_SELL;
                }
            } else {
                g_world_shop_fitting_room_unit_chosen = 0;
                world_formation_init_selection_cursor(g_unit_selected_index);
                g_world_formation_unit_cycle_mode = 0;
                g_world_formation_unit_browse_enabled = 0;
                g_world_shop_cost_window_visible = 0;
                if (g_world_menu_selection_results[1] == 4) {
                    g_world_formation_unit_banner_enabled = 0;
                    g_world_shop_menu_step = WORLD_SHOP_STEP_FITTING_ROOM_CHECKOUT;
                }
            }
            menu_init_selection_value(WORLD_MENU_CURSOR_REMOVE_SLOT, 0);
            menu_init_selection_value(WORLD_MENU_CURSOR_EQUIP_SLOT, 0);
        }
    }
    if (g_world_shop_cost_window_visible != 0) {
        world_shop_install_callbacks_and_run();
    }
}

/* Shop fitting-room equipment step, the shop twin of world_formation_run_equip_item_menu.
 *
 * With the slot list active, steps the equipment-slot cursor and shows the
 * equipped item's description; confirming opens the shop item list for the
 * slot. In the item list, confirming an equippable item returns the unit's
 * current item to the fitting room, equips the shop item (bit 15 marks it as
 * unpaid), adds its price to the fitting-room cost and recalculates stats,
 * while moving the cursor previews the stat change. Cancel leaves the list,
 * or the step (g_world_shop_menu_step = 6) from the slot list. */
void world_shop_run_fitting_room_equip_menu(void) {
    s16 x;
    s16 y;
    s32 prev;
    s32 slot;
    s32 value;
    u16 item;
    s16 equip;
    s32 result;
    s16* cursor_y;

    if (g_world_shop_fitting_equip_menu_initialized == 0) {
        g_world_shop_fitting_equip_menu_initialized = 1;
        g_world_shop_fitting_room_previewed_cursor = -1;
        g_world_shop_fitting_room_item_list_open = 0;
        g_menu_list_selected_index = 0;
    }
    if (g_menu_input_primary_repeat & PSX_PAD_CROSS) {
        g_sound_queued_effect_id = MAIN_SFX_CANCEL;
        if (g_world_shop_fitting_room_item_list_open != 0) {
            g_world_shop_fitting_item_list_close = 1;
        } else {
            g_world_shop_fitting_equip_menu_initialized = 0;
            g_world_shop_menu_step = WORLD_SHOP_STEP_FITTING_ROOM;
        }
    } else if (g_world_shop_fitting_room_item_list_open == 0) {
        prev = g_world_shop_fitting_equip_slot;
        g_world_shop_fitting_equip_slot = menu_update_vertical_selection_and_mark_change(WORLD_EQUIPMENT_SLOT_COUNT,
            WORLD_MENU_CURSOR_EQUIP_SLOT, g_menu_input_primary_repeat, MAIN_SFX_PAGE_SWITCH);
        if (prev != g_world_shop_fitting_equip_slot) {
            g_menu_list_selected_index = 0;
        }
        if (g_world_unit_pointers[g_unit_selected_index]->equipment[g_world_shop_fitting_equip_slot]
            != ITEM_ID_NOTHING) {
            value = (g_world_unit_pointers[g_unit_selected_index]->equipment[g_world_shop_fitting_equip_slot]
                        & WORLD_ITEM_ID_MASK)
                + 0x6800;
        } else {
            value = -1;
        }
        g_menu_help_message_text_id = value;
        if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
            g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
            g_world_formation_unit_cycle_mode = 1;
            g_world_shop_fitting_room_item_list_open = 1;
            g_world_shop_fitting_room_previewed_cursor = -1;
            g_world_shop_cost_window_visible = 0;
            g_world_change_banner_panel_thread_params.style = 1;
            world_shop_build_item_list(g_unit_selected_index, g_world_menu_context_id, g_world_shop_fitting_equip_slot,
                g_item_list_entries, ITEM_LIST_MODE_MARK_UNEQUIPPABLE);
            slot = 0;
            if (g_world_shop_fitting_equip_slot != 0) {
                slot = g_world_shop_fitting_equip_slot - 1;
            }
            menu_get_selection_record_variant(slot, &x, &y, g_item_list_entries);
            menu_init_scrollable_list(g_item_list_entries, x, y, g_text_section_pointers[TEXT_SECTION_ITEM_NAMES]);
            g_world_menu_script_callbacks[0].value = menu_get_entry_item_equipped_count;
            g_world_menu_script_callbacks[1].value = menu_get_entry_item_count;
            g_world_menu_script_callbacks[2].icon = menu_build_item_type_icon_source;
            g_world_menu_script_callbacks[3].icon = menu_get_entry_item_icon_source;
            g_world_menu_script_callbacks[4].value = world_menu_get_item_price_display_value;
            menu_set_vsync_mode_and_event_speed(2);
        }
    } else {
        item = g_item_list_entries[g_menu_list_selected_index];
        if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
            if (g_menu_list_entry_count == 0 || (item & WORLD_ITEM_LIST_ENTRY_UNEQUIPPABLE)) {
                g_sound_queued_effect_id = MAIN_SFX_INVALID;
            } else {
                equip = item;
                result = world_formation_can_equip_item_in_slot(
                    g_unit_selected_index, g_world_shop_fitting_equip_slot, equip);
                if (result < 0) {
                    g_sound_queued_effect_id = MAIN_SFX_INVALID;
                } else {
                    world_shop_return_unit_equip_to_fitting_room(
                        g_unit_selected_index, g_world_shop_fitting_equip_slot);
                    g_world_unit_pointers[g_unit_selected_index]->equipment[g_world_shop_fitting_equip_slot]
                        = item | WORLD_EQUIPMENT_SHOP_ITEM;
                    world_shop_add_fitting_room_cost(world_item_get_price(equip));
                    if (result != 1) {
                        world_shop_return_unit_equip_to_fitting_room(
                            g_unit_selected_index, g_world_shop_fitting_equip_slot == 0);
                    }
                    g_world_shop_fitting_item_list_close = 1;
                    g_sound_queued_effect_id = MAIN_SFX_EQUIP;
                    world_formation_recalculate_selected_unit_stats();
                }
            }
        }
        if (g_menu_list_entry_count != 0 && g_world_shop_fitting_room_previewed_cursor != g_menu_list_selected_index) {
            g_world_shop_fitting_room_previewed_cursor = g_menu_list_selected_index;
            result = world_formation_equip_item_to_unit_slot(
                WORLD_FORMATION_PREVIEW_UNIT_INDEX, g_world_shop_fitting_equip_slot, item);
            if (result == 1) {
                world_menu_toggle_preview_stats_window(1);
                g_menu_preview_stats_thread_params.redraw_request = result;
                item_calculate_equipment_swap_stat_delta(&g_item_preview_stat_detail,
                    &g_menu_selected_unit_stat_summary, g_world_unit_pointers[g_unit_selected_index]->equipment,
                    g_world_formation_temp_unit->equipment);
                bcopy(g_world_unit_pointers[g_unit_selected_index]->equipment, g_world_formation_temp_unit->equipment,
                    10);
            } else {
                world_menu_toggle_preview_stats_window(0);
            }
            g_menu_status_display_thread_params.redraw_request = 1;
        }
        menu_run_script_with_palette_mode_variant(
            &g_world_shop_fitting_item_list_menu_script, g_menu_input_primary_repeat, world_map_is_busy());
        g_menu_help_message_text_id = g_menu_list_entry_count != 0
            ? (g_item_list_entries[g_menu_list_selected_index] & WORLD_ITEM_ID_MASK) + 0x6800
            : -1;
    }
    if (g_world_shop_fitting_item_list_close != 0) {
        g_world_shop_cost_window_visible = 1;
        g_world_shop_fitting_item_list_close = 0;
        g_world_change_banner_panel_thread_params.style = 0;
        g_world_shop_fitting_room_item_list_open = 0;
        g_world_formation_unit_cycle_mode = 2;
        menu_set_vsync_mode_and_event_speed(0);
        world_menu_toggle_preview_stats_window(0);
        g_menu_status_display_thread_params.redraw_request = 1;
        slot = 0;
        if (g_world_shop_fitting_equip_slot != 0) {
            slot = g_world_shop_fitting_equip_slot - 1;
        }
        menu_set_selection_record(slot, g_menu_list_selected_index, g_menu_scroll_base_index, g_item_list_entries);
    }
    cursor_y = &g_world_shop_fitting_slot_cursor_point.y;
    *cursor_y = g_world_shop_fitting_equip_slot * 16 + 0x90;
    gfx_set_otag_index(0x3d);
    menu_update_and_draw_sliding_cursor_variant(&g_world_shop_fitting_slot_cursor_point,
        &g_world_shop_fitting_slot_cursor_anim,
        g_world_shop_fitting_room_item_list_open != 0 || g_menu_input_lock_mode != 0);
    if (g_world_shop_cost_window_visible != 0) {
        world_shop_install_callbacks_and_run();
    }
}

s32 world_shop_get_gil_minus_fitting_room_cost(s32 unused_row) {
    s32 gil;

    gil = world_shop_adjust_gil(0);
    return gil - world_shop_add_fitting_room_cost(0);
}

/* Report whether a menu task or thread 6 is still running.
 *
 * Its only caller passes 0, but the body never reads that argument: `$a0` is
 * overwritten with 6 before any use and the result starts from zero. */
s32 world_menu_is_busy(s32 unused) {
    s32 result = 0;

    if (g_menu_input_lock_mode != 0) {
        result = 1;
    } else if (thread_is_running(THREAD_SCREEN_SLOT_MENU) != 0) {
        result = 1;
    }
    return result;
}

/* Run the fitting room's cost display script with its three menu value
 * callbacks installed (gil, fitting cost, gil minus cost); the script gets
 * no input while a menu is busy. */
void world_shop_install_callbacks_and_run(void) {
    g_world_menu_script_callbacks[0].value = world_shop_adjust_gil;
    g_world_menu_script_callbacks[1].value = world_shop_add_fitting_room_cost;
    g_world_menu_script_callbacks[2].value = world_shop_get_gil_minus_fitting_room_cost;
    menu_run_script_with_palette_mode_variant(
        &g_world_shop_fitting_cost_menu_script, g_menu_input_primary_repeat, world_menu_is_busy(0));
}

/* Fitting-room counterpart of world_menu_run_remove_item: takes one equipment
 * slot off the selected unit, refunding a shop item's price from the fitting
 * room cost, and previews the stat change of the slot under the cursor
 * against the unit's own item held in the fitting room.
 *
 * The prologue's $a0 is not an argument register here. The callee at
 * 0x80114d4c is world_shop_update_unit_selection, which takes no
 * arguments; $a0 simply holds the copy of the selected-unit index that the
 * store to g_world_shop_previewed_unit_index consumes, while the compare keeps the load in $v1.
 * A one-argument prototype would pull the load pseudo into $a0 and push the
 * copy into $v1; pins and launders cannot compensate, because the target's
 * copy sits in the beq's delay slot and a tied asm is never a delay-slot
 * candidate. */
void world_shop_run_fitting_room_remove_step(void) {
    /* The target reserves 64 local bytes whose contents are not accessed;
     * saved registers sit at 0x60..0x70 under a -0x78 frame. */
    s32 unused[16];
    s8 slot;
    s32 index;
    s32 value;
    s32 equipped_item;
    s16* cursor_y;
    s16 unit;

    if (g_world_shop_previewed_unit_index != g_unit_selected_index) {
        unit = g_unit_selected_index;
        g_world_shop_previewed_unit_index = unit;
        g_world_shop_equip_slot_preview_initialized = 0;
        world_shop_update_unit_selection();
        menu_input_clear_state();
    }
    if (g_world_shop_equip_slot_preview_initialized == 0) {
        g_world_shop_previewed_equip_slot = -1;
        g_world_shop_equip_slot_preview_initialized = 1;
    }
    if (g_menu_input_primary_repeat & PSX_PAD_CROSS) {
        world_menu_toggle_preview_stats_window(0);
        g_menu_status_display_thread_params.redraw_request = 1;
        g_world_shop_menu_step = WORLD_SHOP_STEP_FITTING_ROOM;
        g_world_shop_equip_slot_preview_initialized = 0;
        g_sound_queued_effect_id = MAIN_SFX_CANCEL;
    }
    slot = menu_update_vertical_selection_and_mark_change(
        WORLD_EQUIPMENT_SLOT_COUNT, WORLD_MENU_CURSOR_REMOVE_SLOT, g_menu_input_primary_repeat, MAIN_SFX_PAGE_SWITCH);
    if (g_world_unit_pointers[g_unit_selected_index]->equipment[(u8)slot] != ITEM_ID_NOTHING) {
        value = (g_world_unit_pointers[g_unit_selected_index]->equipment[(u8)slot] & WORLD_ITEM_ID_MASK) + 0x6800;
    } else {
        value = -1;
    }
    g_menu_help_message_text_id = value;
    cursor_y = &g_world_shop_equip_slot_cursor_point.y;
    *cursor_y = (u8)slot * 16 + 0x90;
    gfx_set_otag_index(0x3D);
    menu_update_and_draw_sliding_cursor_variant(
        &g_world_shop_equip_slot_cursor_point, &g_world_shop_equip_slot_cursor_anim, world_thread_is_task_active(0));
    if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
        equipped_item = g_world_unit_pointers[g_unit_selected_index]->equipment[(u8)slot];
        if ((equipped_item >> 15) == 0) {
            g_sound_queued_effect_id = MAIN_SFX_INVALID;
        } else {
            world_shop_add_fitting_room_cost(-world_item_get_price(equipped_item & WORLD_ITEM_ID_MASK));
            world_shop_finalize_unit_equips_from_fitting_room(g_unit_selected_index, slot & 0xFF);
            world_formation_recalculate_selected_unit_stats();
            g_sound_queued_effect_id = MAIN_SFX_UNEQUIP;
            g_world_shop_previewed_equip_slot = -1;
        }
    }
    index = (u8)slot;
    if (g_world_shop_previewed_equip_slot != index) {
        equipped_item = g_world_unit_pointers[g_unit_selected_index]->equipment[index];
        g_world_shop_previewed_equip_slot = slot;
        if ((equipped_item >> 15) != 0) {
            world_menu_toggle_preview_stats_window(1);
            menu_calculate_swap_stat_delta(&g_item_preview_stat_detail, &g_menu_selected_unit_stat_summary,
                equipped_item, g_world_shop_fitting_room_items[g_unit_selected_index][index], index);
            g_menu_preview_stats_thread_params.redraw_request = 1;
        } else {
            world_menu_toggle_preview_stats_window(0);
        }
        g_menu_status_display_thread_params.redraw_request = 1;
    }
    if (g_world_shop_cost_window_visible != 0) {
        world_shop_install_callbacks_and_run();
    }
}

/*
 * Fitting room step that prices the tried-on equipment.
 *
 * Runs the checkout prompt: done returns to the shop's main menu (message
 * 0xf802); canceled shows message 0xf818 until any face button, which
 * returns to the fitting room with the unit's stats recalculated.
 */
void world_shop_run_fitting_room_checkout_step(void) {
    s32 result;

    g_world_shop_background_visible = 1;
    if (g_world_shop_checkout_complete != 0) {
        if (g_menu_input_primary_repeat & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE)) {
            g_world_shop_menu_step = WORLD_SHOP_STEP_FITTING_ROOM;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
            g_world_shop_checkout_complete = 0;
            g_world_formation_unit_banner_enabled = 1;
            world_formation_recalculate_selected_unit_stats();
        }
    } else {
        result = world_shop_run_fitting_room_checkout_prompt();
        if (result == WORLD_SHOP_CHECKOUT_DONE) {
            g_world_shop_menu_step = WORLD_SHOP_STEP_MAIN_MENU;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf802, 0);
            g_world_shop_fitting_room_initialized = 0;
        } else if (result == WORLD_SHOP_CHECKOUT_CANCELED) {
            g_world_shop_checkout_complete = 1;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf818, 0);
        }
    }
    world_shop_run_obtain_gil_menu();
}

/* Pay and cannot-afford prompt menus; their cursors are reset on entry. */

/*
 * Run one frame of the fitting-room checkout prompt.
 *
 * Opens the pay prompt when the party can afford the fitting-room cost and
 * the "not enough gil" prompt otherwise; an empty cart is bought
 * immediately. Returns 1 while a prompt is open; 0 and -1 are the two
 * closing outcomes world_shop_run_fitting_room_checkout_step dispatches on.
 */
s32 world_shop_run_fitting_room_checkout_prompt(void) {
    s32 balance;
    s32 cost;

    if (g_world_shop_checkout_prompt_initialized == 0) {
        if (thread_is_running(THREAD_SCREEN_SLOT_MESSAGE) != 0) {
            return WORLD_SHOP_CHECKOUT_PROMPT_OPEN;
        }
        g_world_shop_checkout_confirm_menu.selected_index = 0;
        g_world_shop_checkout_short_gil_menu.selected_index = 0;
        thread_start(THREAD_SCREEN_SLOT_MESSAGE, world_text_message_box_thread);
        balance = world_shop_adjust_gil(0) - (cost = world_shop_add_fitting_room_cost(0));
        if (cost == 0) {
            world_shop_buy_from_fitting_room();
            return WORLD_SHOP_CHECKOUT_DONE;
        }
        if (balance >= 0) {
            g_sound_suppressed = 1;
            g_world_shop_checkout_prompt_menu = &g_world_shop_checkout_confirm_menu;
            g_text_substitution_values[0] = cost;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf819, 0);
            g_world_shop_checkout_insufficient_gil = 0;
        } else {
            g_world_shop_checkout_prompt_menu = &g_world_shop_checkout_short_gil_menu;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf81a, 0);
            g_world_shop_checkout_insufficient_gil = 1;
        }
        g_world_shop_checkout_prompt_initialized = 1;
        g_world_shop_checkout_prompt_active = 1;
    }
    if (g_world_shop_checkout_prompt_active != 0) {
        g_world_shop_checkout_prompt_active
            = menu_start_managed_thread(WORLD_THREAD_SLOT_UPPER_MENU, g_world_shop_checkout_prompt_menu);
        if (g_world_shop_checkout_prompt_active != 0) {
            return WORLD_SHOP_CHECKOUT_PROMPT_OPEN;
        }
        g_world_shop_checkout_prompt_result = g_world_menu_selection_results[WORLD_MENU_SELECTION_PROMPT_RESULT];
        if (g_world_shop_checkout_insufficient_gil == 0) {
            g_sound_suppressed = 0;
            if (g_world_shop_checkout_prompt_result == 0) {
                balance = -world_shop_add_fitting_room_cost(0);
                world_shop_adjust_gil(balance);
                world_shop_add_fitting_room_cost(balance);
                world_shop_buy_from_fitting_room();
                world_formation_save_records_to_party();
                g_sound_queued_effect_id = MAIN_SFX_SHOP_TRANSACTION;
                item_rebuild_category_list(0);
                item_rebuild_category_list(1);
                item_rebuild_category_list(2);
                item_rebuild_category_list(3);
                item_rebuild_category_list(4);
                g_world_shop_checkout_prompt_initialized = 0;
                return WORLD_SHOP_CHECKOUT_DONE;
            }
            if (g_world_shop_checkout_prompt_result == 1) {
                g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
            }
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf81c, 0);
            return WORLD_SHOP_CHECKOUT_PROMPT_OPEN;
        }
        if (g_world_shop_checkout_prompt_result != 1) {
            world_shop_apply_fitting_room_items_to_all_units();
            g_world_shop_checkout_prompt_initialized = 0;
            return WORLD_SHOP_CHECKOUT_DONE;
        }
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
        g_world_shop_checkout_prompt_initialized = 0;
        return WORLD_SHOP_CHECKOUT_CANCELED;
    }
    if (menu_start_managed_thread(WORLD_THREAD_SLOT_UPPER_MENU, &g_world_shop_checkout_short_gil_menu) != 0) {
        return WORLD_SHOP_CHECKOUT_PROMPT_OPEN;
    }
    g_world_shop_checkout_prompt_initialized = 0;
    if (g_world_menu_selection_results[WORLD_MENU_SELECTION_PROMPT_RESULT] != 1) {
        if (g_world_shop_checkout_prompt_result != 0) {
            world_shop_apply_fitting_room_items_to_all_units();
            return WORLD_SHOP_CHECKOUT_DONE;
        }
        return WORLD_SHOP_CHECKOUT_DONE;
    }
    thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
    return WORLD_SHOP_CHECKOUT_CANCELED;
}

/* Move every reserved fitting-room item onto its formation unit and clear
 * the reservation. Equipment slots flagged in bit 14 are overwritten even
 * when no item was reserved. */
void world_shop_apply_fitting_room_items_to_all_units(void) {
    s32 i;
    s32 j;

    for (i = 0; i < g_world_formation_unit_count; i++) {
        for (j = 0; j < WORLD_EQUIPMENT_SLOT_COUNT; j++) {
            if ((g_world_unit_pointers[i]->equipment[j] >> 14) != 0
                || g_world_shop_fitting_room_items[i][j] != ITEM_ID_NOTHING) {
                g_world_unit_pointers[i]->equipment[j] = g_world_shop_fitting_room_items[i][j];
            }
            g_world_shop_fitting_room_items[i][j] = ITEM_ID_NOTHING;
        }
    }
}

/*
 * Shop "best fit / fitting room" preview step, entry 8 of the shop callback
 * table at 0x8019525c.
 *
 * On entry it stages the selected unit's four weapon/armour slots, fills any
 * slot already taken from the shop with that slot's fitting-room reservation,
 * asks 0x80123dd8 for a suggested set, restores the staged slots and previews
 * the resulting stat delta. It then runs the confirmation menu; on confirm it
 * commits the suggested set slot by slot, returning each displaced item to the
 * fitting room or refunding a shop item's price, and returns to shop step 6.
 *
 * The gate at 0x801957a8 is `s8`, not `u8` (the target sign-loads it, `lb`).
 * Loop 2 computes both the suggested item and the unit's own slot at the
 * dominator; deferring either load into an arm costs a duplicate load and the
 * hoist of the selected-unit index.
 *
 * `item` is s32. As `s16` it is an HImode pseudo, so gcc emits a truncation
 * copy (`move s3,s0`) and stores through the copy while testing and passing
 * the original: seven callee-saved registers where the target uses six
 * (s0-s4 + ra), an extra save and a 136-byte frame. With `item` wide,
 * `shifted = suggested << 16; item = shifted >> 16;` restores the target's
 * `lhu` / `sll` / `sra` triple and lets the shifted value serve the sign test
 * (`bgez`); an HImode value folds the pair into one `lh`.
 *
 * The frame pad is split around the staging buffer -- `unused[16]`, `staged`,
 * `unused_tail[2]` -- to keep the buffer at the target's sp+80
 * (`.frame vars=88, regs=6/0, args=16`); a single `unused[18]` pushes it to
 * sp+88.
 *
 * The target has `move s1,v0` after world_menu_run_thread and no
 * initialization of s1 at loop 2's head: loop 2's counter is the run_thread
 * result's allocno, so the source assigns the result to `i` and the loop is
 * written `for (; i < WORLD_EQUIPMENT_SLOT_ACCESSORY; i++)`. A fresh block-local takes a call-clobbered
 * register; reusing a function-scope allocno that is already dead puts the
 * counter in the target's callee-saved s1 and decides which of the two loop
 * quantities is allocated first.
 *
 * In the loop-1 preheader the target computes
 * `&g_world_unit_pointers[idx]` first (into a1) and the
 * fitting-room row base second (into a0). Giving the unit slot its own local
 * (`unit_slot`), assigned just before `reserved`, and reading the loop body
 * through `(*unit_slot)->equipment[i]`, puts the two preheader address
 * computations in that order. The `lw` stays inside the loop, since the store
 * through `reserved` may alias the pointer array.
 *
 * Within `reserved`'s address expression the target materialises the
 * fitting-room base `lui`/`addiu` before the `idx*5*2` index arithmetic. cc1
 * itself emits `addu`/`sll`/`la`/`addu` in the other order under one `.loc`,
 * because the symbol canonicalises to the second operand of the address PLUS
 * and is expanded last; no operand respelling changes that (`fold` rewrites
 * `base - -(idx * 5)` straight back to a PLUS). Giving the row base its own
 * local and its own statement (`room_base = &g_world_shop_fitting_room_items[0][0];`
 * then `reserved = room_base + idx * 5;`) forces the `la` to be emitted at
 * that statement, ahead of the index arithmetic.
 *
 * The confirmation result g_world_menu_selection_results[5] is read directly
 * each time. Its address stays in one register across the calls and the last
 * read is scheduled ahead of the two byte stores that precede it in source.
 */
void world_shop_run_best_fit_preview_step(void) {
    /* The target reserves vars the code never touches: args 16 + vars 88 +
     * 6 saves 24 = the frame of 128, while only the 8-byte staging buffer at
     * sp+80 is accessed. */
    s32 unused[16];
    u16 staged[4];
    s32 unused_tail[2];
    s32 i;
    s32 item;

    if (g_world_shop_best_fit_menu_open == 0) {
        world_unit_t** unit_slot;
        s16* room_base;
        s16* reserved;

        g_world_shop_best_fit_menu.selected_index = 0;
        bcopy(g_world_unit_pointers[g_unit_selected_index]->equipment, staged, sizeof(staged));
        unit_slot = &g_world_unit_pointers[g_unit_selected_index];
        room_base = &g_world_shop_fitting_room_items[0][0];
        reserved = room_base + g_unit_selected_index * WORLD_EQUIPMENT_SLOT_COUNT;
        for (i = 0; i < WORLD_EQUIPMENT_SLOT_ACCESSORY; i++, reserved++) {
            if (((*unit_slot)->equipment[i] >> 15) != 0) {
                (*unit_slot)->equipment[i] = *reserved;
            }
        }
        world_shop_build_fitting_room_best_fit_equipment(
            g_unit_selected_index, g_world_menu_context_id, g_item_preview_stat_detail.state.equipment);
        g_item_preview_stat_detail.state.equipment[WORLD_EQUIPMENT_SLOT_ACCESSORY]
            = g_world_unit_pointers[g_unit_selected_index]->equipment[WORLD_EQUIPMENT_SLOT_ACCESSORY];
        bcopy(staged, g_world_unit_pointers[g_unit_selected_index]->equipment, sizeof(staged));
        item_calculate_equipment_swap_stat_delta(&g_item_preview_stat_detail, &g_menu_selected_unit_stat_summary,
            g_world_unit_pointers[g_unit_selected_index]->equipment, (u16*)g_item_preview_stat_detail.state.equipment);
        g_item_preview_stat_detail.state.two_hands = unit_is_two_handing_weapon(
            g_item_preview_stat_detail.state.equipment, world_ability_has_two_hands(g_unit_selected_index));
        menu_toggle_stat_preview_panel_thread(1);
        world_menu_toggle_preview_stats_window(1);
        g_menu_status_display_thread_params.redraw_request = 1;
        g_menu_preview_stats_thread_params.redraw_request = 1;
        g_world_shop_best_fit_menu_open = 1;
        g_world_formation_unit_cycle_mode = 1;
        g_sound_suppressed = 1;
    }
    i = menu_start_managed_thread(THREAD_SCREEN_SLOT_MENU, &g_world_shop_best_fit_menu);
    if (i == 0) {
        if (g_world_menu_selection_results[5] == 0) {
            g_sound_queued_effect_id = MAIN_SFX_EQUIP;
        }
        if (g_world_menu_selection_results[5] == 1) {
            g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
        }
        menu_toggle_stat_preview_panel_thread(0);
        world_menu_toggle_preview_stats_window(0);
        g_menu_status_display_thread_params.redraw_request = 1;
        g_world_shop_best_fit_menu_open = 0;
        g_world_formation_unit_cycle_mode = 2;
        if (g_world_menu_selection_results[5] == 0) {
            for (; i < WORLD_EQUIPMENT_SLOT_ACCESSORY; i++) {
                u16 suggested;
                s32 shifted;
                s32 current;

                /* The target reads the suggested item unsigned and widens it
                 * explicitly, reusing the shifted value for the sign test
                 * (`lhu` / `sll 16` / `sra 16`, then `bgez` on the shift), and
                 * it loads the unit's own slot ONCE at the loop head -- the
                 * `sra ,0xf` lands in the `bgez` delay slot and the same
                 * register is sign-extended again for world_item_get_price. */
                suggested = g_item_preview_stat_detail.state.equipment[i];
                shifted = suggested << 16;
                item = shifted >> 16;
                current = g_world_unit_pointers[g_unit_selected_index]->equipment[i];
                if (item == 0 || shifted < 0) {
                    world_shop_return_unit_equip_to_fitting_room(g_unit_selected_index, i);
                    world_shop_add_fitting_room_cost(world_item_get_price(item));
                } else if ((current >> 15) != 0) {
                    world_shop_add_fitting_room_cost(-world_item_get_price(current));
                    g_world_shop_fitting_room_items[g_unit_selected_index][i] = ITEM_ID_NOTHING;
                }
                g_world_unit_pointers[g_unit_selected_index]->equipment[i] = item;
            }
            world_formation_recalculate_selected_unit_stats();
        }
        g_world_shop_menu_step = WORLD_SHOP_STEP_FITTING_ROOM;
        g_sound_suppressed = 0;
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
    }
    if (g_world_shop_cost_window_visible != 0) {
        world_shop_install_callbacks_and_run();
    }
}

/*
 * Run one frame of the shop's sell-equipped-item menu.
 *
 * The cursor walks the selected unit's five equipment slots; confirming a
 * slot the unit owns (non-empty, not taken from the shop) opens a sale prompt
 * for half the item's price and, on acceptance, credits the gil and clears
 * the slot. Triangle returns to shop step 6.
 */
void world_shop_run_sell_equipped_item_menu(void) {
    s32 close;
    u32 item;
    s32 text_id;
    u32 input;

    close = 0;
    if (g_world_shop_sell_equipped_menu_open == 0) {
        if (thread_is_running(THREAD_SCREEN_SLOT_MESSAGE) == 0) {
            thread_start(THREAD_SCREEN_SLOT_MESSAGE, world_text_message_box_thread);
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf80c, 0);
            g_world_shop_sell_equipped_prompt_active = 0;
            g_world_formation_unit_cycle_mode = 1;
            g_world_shop_sell_equipped_menu_open = 1;
            g_world_shop_sell_equipped_result_shown = 0;
        }
    } else {
        if (g_world_shop_sell_equipped_result_shown != 0) {
            if (g_menu_input_primary_repeat
                & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE | PSX_PAD_DPAD_MASK)) {
                thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf80d, 0);
                g_world_shop_sell_equipped_result_shown = 0;
            }
        } else if (g_world_shop_sell_equipped_prompt_active == 0) {
            g_world_shop_sell_equipped_slot = menu_update_vertical_selection_and_mark_change(WORLD_EQUIPMENT_SLOT_COUNT,
                WORLD_MENU_CURSOR_REMOVE_SLOT, g_menu_input_primary_repeat, MAIN_SFX_PAGE_SWITCH);
            if (g_world_unit_pointers[g_unit_selected_index]->equipment[g_world_shop_sell_equipped_slot]
                != ITEM_ID_NOTHING) {
                text_id = (g_world_unit_pointers[g_unit_selected_index]->equipment[g_world_shop_sell_equipped_slot]
                              & WORLD_ITEM_ID_MASK)
                    + 0x6800;
            } else {
                text_id = -1;
            }
            input = g_menu_input_primary_repeat;
            g_menu_help_message_text_id = text_id;
            if (input & PSX_PAD_CIRCLE) {
                item = g_world_unit_pointers[g_unit_selected_index]->equipment[g_world_shop_sell_equipped_slot];
                if (item == ITEM_ID_NOTHING || (item >> 15) != 0) {
                    g_sound_queued_effect_id = MAIN_SFX_INVALID;
                } else {
                    g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
                    g_world_shop_sell_equipped_prompt_active = 1;
                    g_world_shop_sell_equipped_menu.selected_index = 0;
                    g_text_substitution_values[0] = world_item_get_half_price(item);
                    thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf810, 0);
                }
            } else if (input & PSX_PAD_CROSS) {
                close = 1;
            }
        } else {
            g_menu_help_message_text_id = 0;
            g_sound_suppressed = g_world_shop_sell_equipped_prompt_active
                = menu_start_managed_thread(THREAD_SCREEN_SLOT_MENU, &g_world_shop_sell_equipped_menu);
            if (g_world_shop_sell_equipped_prompt_active == 0) {
                if (g_world_menu_selection_results[WORLD_MENU_SELECTION_PROMPT_RESULT] == 0) {
                    thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf811, 0);
                    world_shop_adjust_gil(g_text_substitution_values[0]);
                    g_world_unit_pointers[g_unit_selected_index]->equipment[g_world_shop_sell_equipped_slot]
                        = ITEM_ID_NOTHING;
                    world_formation_recalculate_selected_unit_stats();
                    g_sound_queued_effect_id = MAIN_SFX_SHOP_TRANSACTION;
                } else {
                    if (g_world_menu_selection_results[WORLD_MENU_SELECTION_PROMPT_RESULT] == 1) {
                        g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
                    }
                    thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf80a, 0);
                }
                g_world_shop_sell_equipped_result_shown = 1;
            }
        }
        if (close != 0) {
            g_sound_queued_effect_id = MAIN_SFX_CANCEL;
            g_world_formation_unit_cycle_mode = 2;
            g_world_shop_sell_equipped_menu_open = 0;
            g_world_shop_menu_step = WORLD_SHOP_STEP_FITTING_ROOM;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
        }
        g_world_shop_sell_equipped_cursor_point.y = g_world_shop_sell_equipped_slot * 16 + 0x90;
        gfx_set_otag_index(0x3d);
        menu_update_and_draw_sliding_cursor_variant(&g_world_shop_sell_equipped_cursor_point,
            &g_world_shop_sell_equipped_cursor_anim, world_thread_is_task_active(0));
    }
    world_shop_install_callbacks_and_run();
}

/*
 * Run the menu's state-4/state-5 subflow through thread 2.
 *
 * State 0 closes the subflow and hands control back as state 6; otherwise
 * the updated state is retained for the next call. The menu role is unverified.
 */
void world_shop_run_sell_menu_subflow_step(void) {
    if (g_world_shop_sell_subflow_initialized == 0) {
        if (thread_is_running(THREAD_SCREEN_SLOT_MESSAGE) != 0) {
            return;
        }
        thread_start(THREAD_SCREEN_SLOT_MESSAGE, world_text_message_box_thread);
        g_world_formation_unit_browse_enabled = 0;
        g_world_formation_unit_banner_enabled = 0;
        world_formation_stop_menu_threads();
        g_world_formation_unit_cycle_mode = 0;
        g_world_shop_sell_subflow_step = WORLD_SHOP_STEP_SELL_LIST;
        menu_clear_selection_record(5);
        menu_clear_selection_record(6);
        menu_clear_selection_record(7);
        menu_clear_selection_record(8);
        menu_clear_selection_record(9);
        g_world_shop_sell_subflow_initialized = 1;
    }
    g_world_shop_menu_step = g_world_shop_sell_subflow_step;
    if (g_world_shop_sell_subflow_step == WORLD_SHOP_STEP_SELL_LIST) {
        world_shop_run_item_sell_list_step();
        if (g_world_shop_sell_subflow_step != g_world_shop_menu_step) {
            world_menu_reset_selection_results();
        }
    } else if (g_world_shop_sell_subflow_step == WORLD_SHOP_STEP_SELL_CONFIRM) {
        world_shop_run_item_sell_confirm_step();
    }
    if (g_world_shop_menu_step == WORLD_SHOP_STEP_MAIN_MENU) {
        g_world_shop_menu_step = WORLD_SHOP_STEP_FITTING_ROOM;
        g_world_formation_unit_cycle_mode = 2;
        g_world_shop_sell_subflow_initialized = 0;
        g_world_formation_unit_browse_enabled = 1;
        menu_set_vsync_mode_and_event_speed(2);
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
        return;
    }
    g_world_shop_sell_subflow_step = g_world_shop_menu_step;
    g_world_shop_menu_step = WORLD_SHOP_STEP_FITTING_ROOM_SELL;
}

/* Clears one equipment slot of a formation unit.  An item the unit owned goes
 * back to the fitting room reserve; an item taken from the shop (bit 15 set)
 * is refunded from the fitting room cost instead. */
void world_shop_return_unit_equip_to_fitting_room(s16 unit_index, s16 equipment_index) {
    s32 equipped_item = g_world_unit_pointers[unit_index]->equipment[equipment_index];

    if (equipped_item != ITEM_ID_NOTHING) {
        if ((equipped_item >> 15) == 0)
            g_world_shop_fitting_room_items[unit_index][equipment_index] = equipped_item;
        else
            world_shop_add_fitting_room_cost(-world_item_get_price(equipped_item & WORLD_ITEM_ID_MASK));
        g_world_unit_pointers[unit_index]->equipment[equipment_index] = ITEM_ID_NOTHING;
    }
}

/* Finalize a unit's equipment from the Fitting Room. */
void world_shop_finalize_unit_equips_from_fitting_room(s16 formation_index, s16 equipment_index) {
    u16 reserved_item;

    g_world_unit_pointers[formation_index]->equipment[equipment_index] = ITEM_ID_NOTHING;
    reserved_item = g_world_shop_fitting_room_items[formation_index][equipment_index];
    if (equipment_index < WORLD_EQUIPMENT_SLOT_HEAD) {
        u16 right_hand = g_world_unit_pointers[formation_index]->equipment[WORLD_EQUIPMENT_SLOT_RIGHT_HAND];
        u16 left_hand = g_world_unit_pointers[formation_index]->equipment[WORLD_EQUIPMENT_SLOT_LEFT_HAND];

        if (right_hand == ITEM_ID_NOTHING && left_hand == ITEM_ID_NOTHING) {
            g_world_unit_pointers[formation_index]->equipment[WORLD_EQUIPMENT_SLOT_RIGHT_HAND]
                = g_world_shop_fitting_room_items[formation_index][WORLD_EQUIPMENT_SLOT_RIGHT_HAND];
            g_world_unit_pointers[formation_index]->equipment[WORLD_EQUIPMENT_SLOT_LEFT_HAND]
                = g_world_shop_fitting_room_items[formation_index][WORLD_EQUIPMENT_SLOT_LEFT_HAND];
            g_world_shop_fitting_room_items[formation_index][WORLD_EQUIPMENT_SLOT_RIGHT_HAND] = ITEM_ID_NOTHING;
            g_world_shop_fitting_room_items[formation_index][WORLD_EQUIPMENT_SLOT_LEFT_HAND] = ITEM_ID_NOTHING;
            return;
        }
        if (reserved_item != ITEM_ID_NOTHING
            && world_formation_can_equip_item_in_slot(g_unit_selected_index, equipment_index, reserved_item) == 1) {
            g_world_unit_pointers[formation_index]->equipment[equipment_index] = reserved_item;
            g_world_shop_fitting_room_items[formation_index][equipment_index] = ITEM_ID_NOTHING;
        }
    } else {
        g_world_unit_pointers[formation_index]->equipment[equipment_index] = reserved_item;
        g_world_shop_fitting_room_items[formation_index][equipment_index] = ITEM_ID_NOTHING;
    }
}

/* Complete a fitting-room purchase: every tried-on item (entry bits 14-15
 * set) becomes the unit's own, and the equipment it displaced, kept in
 * g_world_shop_fitting_room_items, goes back to the inventory. */
void world_shop_buy_from_fitting_room(void) {
    s32 i;
    s32 j;

    for (i = 0; i < g_world_formation_unit_count; i++) {
        for (j = 0; j < WORLD_EQUIPMENT_SLOT_COUNT; j++) {
            s32 equipped_item = g_world_unit_pointers[i]->equipment[j];
            if ((equipped_item >> 14) != 0) {
                g_world_unit_pointers[i]->equipment[j] &= WORLD_ITEM_ID_MASK;
            }
            item_adjust_inventory_count(g_world_shop_fitting_room_items[i][j], 1);
            g_world_shop_fitting_room_items[i][j] = ITEM_ID_NOTHING;
        }
    }
}
