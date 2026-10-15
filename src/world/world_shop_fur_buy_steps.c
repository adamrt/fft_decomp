/* Half-price value and the fur shop buy steps. */

#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Half the menu item's price, with bit 30 copied from its 0x4000 entry flag. */
/* Target 0x801379cc. */
s32 world_menu_get_item_half_price_display_value(s32 entry_index) {
    s32 display_value = world_item_get_price(g_item_list_entries[entry_index]) >> 1;

    if ((g_item_list_entries[entry_index] & WORLD_ITEM_LIST_ENTRY_UNEQUIPPABLE) != 0)
        display_value |= MENU_NUMBER_VALUE_DISABLED;
    return display_value;
}

/* Fur shop: the list of the shop's poached stock, bought at half price.
 *
 * Mirrors world_shop_handle_item_purchase_menu with the half-price value and
 * the poached-stock quantity as callbacks. */
void world_shop_run_fur_buy_list_step(void) {
    s16 x;
    s16 y;
    s32 input;
    s32 disabled;
    s8 result;

    if (g_world_shop_fur_purchase_list_initialized == 0) {
        if (thread_is_running(THREAD_SCREEN_SLOT_MESSAGE) == 0) {
            thread_start(THREAD_SCREEN_SLOT_MESSAGE, world_text_message_box_thread);
        }
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF832, 0);
        g_world_shop_fur_purchase_list_initialized = 1;
        world_shop_build_item_list(-1, g_world_menu_context_id,
            g_world_shop_item_category != 4 ? g_world_shop_item_category + 1 : ITEM_LIST_FILTER_OTHER,
            g_item_list_entries, ITEM_LIST_MODE_ALL);
        menu_get_selection_record_variant(g_world_shop_item_category, &x, &y, g_item_list_entries);
        menu_init_scrollable_list(g_item_list_entries, x, y, g_text_section_pointers[TEXT_SECTION_ITEM_NAMES]);
        g_world_menu_script_callbacks[0].value = menu_get_entry_item_equipped_count;
        g_world_menu_script_callbacks[1].value = menu_get_entry_item_count;
        g_world_menu_script_callbacks[2].icon = menu_build_item_type_icon_source;
        g_world_menu_script_callbacks[3].icon = menu_get_entry_item_icon_source;
        g_world_menu_script_callbacks[4].value = world_menu_get_item_half_price_display_value;
        g_world_menu_script_callbacks[5].value = world_item_count_selected_equipped_by_party;
        g_world_menu_script_callbacks[6].value = world_item_count_selected_owned_and_equipped;
        g_world_menu_script_callbacks[8].value = world_shop_get_selected_poached_item_quantity;
        g_world_shop_fur_equip_check_pending = 0;
    }
    menu_set_vsync_mode_and_event_speed(2);
    if (g_world_shop_fur_equip_check_pending != 0) {
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
        result = thread_is_running(THREAD_SCREEN_SLOT_MESSAGE);
        g_world_shop_fur_equip_check_pending = result;
        if (result == 0) {
            menu_set_selection_record(
                g_world_shop_item_category, g_menu_list_selected_index, g_menu_scroll_base_index, g_item_list_entries);
            g_world_shop_menu_step = WORLD_SHOP_STEP_FUR_BUY_EQUIP;
            g_world_shop_fur_purchase_list_initialized = 0;
        }
        menu_input_clear_state();
    } else {
        input = g_menu_input_primary_repeat;
        if (input & PSX_PAD_CROSS) {
            g_world_shop_fur_purchase_list_initialized = 0;
            g_world_shop_menu_step = WORLD_SHOP_STEP_FUR_MENU;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF830, 0);
            g_sound_queued_effect_id = MAIN_SFX_CANCEL;
        } else if ((input & PSX_PAD_TRIANGLE) && g_menu_list_entry_count != 0 && g_world_shop_item_category != 4) {
            g_world_shop_fur_equip_check_pending = 1;
        } else if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
            if (g_menu_list_entry_count == 0) {
                g_sound_queued_effect_id = MAIN_SFX_INVALID;
            } else {
                g_world_shop_menu_step = WORLD_SHOP_STEP_FUR_BUY_CONFIRM;
                g_world_shop_quantity_menu_data[1].selected_index = 0;
                g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
            }
        }
    }
    disabled = 0;
    world_menu_switch_item_category(g_menu_input_secondary_repeat);
    menu_run_script_with_palette_mode_variant(
        &g_world_shop_fur_purchase_menu_script, g_menu_input_primary_repeat, g_menu_input_lock_mode);
    if (world_map_is_busy() != 0 || g_menu_list_entry_count == 0 || g_world_shop_item_category == 4) {
        disabled = 1;
    }
    world_menu_draw_pressable_button(MENU_PRESSABLE_BUTTON_SHOP_LIST_TRIANGLE, 0xBA, 0x4C,
        g_menu_input_primary_repeat & PSX_PAD_TRIANGLE, disabled, MAIN_SFX_CONFIRM);
    world_shop_run_obtain_gil_menu();
}

/* Fur shop sibling of world_menu_confirm_item_purchase: the party pays half
 * price and the poached stock is reduced. */
void world_shop_run_fur_buy_confirm_step(void) {
    s32 result = 1;

    if (g_world_shop_fur_purchase_message_wait != 0) {
        if (g_menu_input_primary_repeat
            & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE | PSX_PAD_DPAD_MASK)) {
            g_world_shop_fur_purchase_message_wait = 0;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF83B, 0);
            g_world_shop_menu_step = WORLD_SHOP_STEP_FUR_BUY_LIST;
            g_world_menu_prompt_active = 0;
        }
    } else {
        result = world_shop_run_fur_buy_quantity_menu();
    }
    if (result == 0) {
        if (g_world_menu_selection_results[WORLD_MENU_SELECTION_PROMPT_RESULT] == 0) {
            world_shop_adjust_gil(-(world_item_get_price(g_item_list_entries[g_menu_list_selected_index]) >> 1)
                * g_world_menu_selection_results[WORLD_MENU_SELECTION_SHOP_QUANTITY]);
            item_adjust_inventory_count(g_item_list_entries[g_menu_list_selected_index],
                g_world_menu_selection_results[WORLD_MENU_SELECTION_SHOP_QUANTITY]);
            world_shop_adjust_poached_item_quantity(g_item_list_entries[g_menu_list_selected_index],
                -g_world_menu_selection_results[WORLD_MENU_SELECTION_SHOP_QUANTITY]);
            item_rebuild_category_list(g_world_shop_item_category);
            world_shop_build_item_list(-1, g_world_menu_context_id,
                g_world_shop_item_category != 4 ? g_world_shop_item_category + 1 : ITEM_LIST_FILTER_OTHER,
                g_item_list_entries, ITEM_LIST_MODE_ALL);
            world_menu_init_and_load_scrollable_list(g_item_list_entries, g_menu_list_selected_index,
                g_text_section_pointers[TEXT_SECTION_ITEM_NAMES], &g_world_shop_fur_purchase_menu_script);
            menu_set_window_scale_step(10);
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF837, 0);
        } else {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF838, 0);
        }
    }
    if (result <= 0) {
        g_world_shop_fur_purchase_message_wait = 1;
    }
    world_menu_switch_item_category(0);
    menu_run_script_with_palette_mode_variant(
        &g_world_shop_fur_purchase_menu_script, 0, thread_is_running(THREAD_SCREEN_SLOT_TEXT));
    world_menu_draw_pressable_button(MENU_PRESSABLE_BUTTON_SHOP_LIST_TRIANGLE, 0xBA, 0x4C, 0, 1, MAIN_SFX_CONFIRM);
    world_shop_run_obtain_gil_menu();
}

/* Fur shop twin of world_shop_run_item_purchase_quantity_menu: the quantity
 * prompt is capped by the 99-item inventory limit, the gil on hand at half
 * price and the poached stock. */
s32 world_shop_run_fur_buy_quantity_menu(void) {
    s32 owned;
    s32 count;
    s32 price;
    s32 room;
    s32 stock;

    if (g_world_shop_fur_purchase_result == 0) {
        g_world_shop_fur_purchase_confirm_shown = 0;
        g_world_shop_fur_purchase_item = g_item_list_entries[g_menu_list_selected_index];
        g_world_menu_prompt_active = 1;
        owned = item_get_total_count_variant(g_world_shop_fur_purchase_item);
        if (owned >= 99) {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF834, 0);
            return WORLD_SHOP_QUANTITY_HOLDING_MAX;
        }
        count = world_shop_adjust_gil(0);
        price = world_item_get_price(g_world_shop_fur_purchase_item) >> 1;
        count /= price;
        g_world_shop_fur_purchase_price = price;
        room = 99 - owned;
        if (room < count) {
            count = room;
        }
        if (count == 0) {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF833, 0);
            return WORLD_SHOP_QUANTITY_CANNOT_AFFORD;
        }
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF835, 0);
        stock = world_shop_adjust_poached_item_quantity(g_world_shop_fur_purchase_item, 0);
        if (count >= stock) {
            count = stock;
        }
        g_world_shop_quantity_limit = count;
        g_sound_suppressed = 1;
    }
    g_world_shop_fur_purchase_result
        = menu_start_managed_thread(WORLD_THREAD_SLOT_UPPER_MENU, g_world_shop_quantity_menu_data);
    if (thread_is_running(WORLD_THREAD_SLOT_UPPER_MENU_CHILD) != 0 && g_world_shop_fur_purchase_confirm_shown == 0) {
        g_text_substitution_values[0]
            = g_world_menu_selection_results[WORLD_MENU_SELECTION_SHOP_QUANTITY] * g_world_shop_fur_purchase_price;
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF836, 0);
        g_world_shop_fur_purchase_confirm_shown = 1;
        g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
    }
    if (thread_is_running(WORLD_THREAD_SLOT_UPPER_MENU_CHILD) == 0 && g_world_shop_fur_purchase_confirm_shown != 0) {
        g_world_shop_fur_purchase_confirm_shown = 0;
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF835, 0);
    }
    if (g_world_shop_fur_purchase_result == 0) {
        if (g_world_menu_selection_results[WORLD_MENU_SELECTION_PROMPT_RESULT] == 0) {
            g_sound_queued_effect_id = MAIN_SFX_SHOP_TRANSACTION;
        }
        if (g_world_menu_selection_results[WORLD_MENU_SELECTION_PROMPT_RESULT] == 1) {
            g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
        }
        g_sound_suppressed = 0;
    }
    return g_world_shop_fur_purchase_result;
}
