/* Fur shop menu and sell steps. */

#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Fur shop menu step: on entry checks for a Secret Hunt unit and poached
 * stock (refusing entry with message 0xf82d when nobody has the ability),
 * then routes the selected entry to the buy list, the sell list or a
 * refusal message. */
void world_shop_run_fur_menu_step(void) {
    s32 i;

    if (g_world_shop_fur_opened == 0) {
        g_menu_list_selected_index = 0;
        menu_set_vsync_mode_and_event_speed(0);
        g_world_shop_fur_has_secret_hunt = 0;
        g_world_shop_fur_has_poached_stock = 0;
        g_world_shop_fur_access_granted = 0;
        g_world_shop_fur_waiting_message = 0;
        for (i = 0; i < g_world_formation_unit_count; i++) {
            if (world_ability_has_secret_hunt(i) != 0) {
                g_world_shop_fur_has_secret_hunt = 1;
            }
        }
        if (g_world_shop_fur_has_secret_hunt != 0) {
            for (i = 1; i < 0x100; i++) {
                if (world_shop_adjust_poached_item_quantity(i, 0) != 0) {
                    g_world_shop_fur_has_poached_stock = 1;
                }
            }
        }
        if (g_world_shop_fur_has_secret_hunt == 0) {
            g_world_shop_menu_step = WORLD_SHOP_STEP_EXIT_FADE;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF82D, 0);
            return;
        }
        g_world_shop_fur_access_granted = 1;
        g_world_shop_fur_opened = 1;
    }
    if (g_world_shop_fur_waiting_message != 0) {
        if (g_menu_input_primary_repeat & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE)) {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF830, 0);
            g_world_shop_fur_waiting_message = 0;
        }
    } else if (menu_start_managed_thread(THREAD_SCREEN_SLOT_MENU, &g_world_shop_fur_menu_thread_data) == 0) {
        menu_clear_selection_records();
        if (g_world_menu_selection_results[0] == 0) {
            if (g_world_shop_fur_has_poached_stock != 0) {
                g_world_shop_item_category = menu_init_cursor_position(WORLD_MENU_CURSOR_SHOP_ITEM_TAB, 0);
                g_world_shop_menu_step = WORLD_SHOP_STEP_FUR_BUY_LIST;
                g_world_shop_fur_opened = 0;
            } else {
                thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF82E, 0);
                g_world_shop_fur_waiting_message = 1;
            }
        } else if (g_world_menu_selection_results[0] == 1) {
            if (world_menu_has_items(0) != 0) {
                g_world_shop_menu_step = WORLD_SHOP_STEP_FUR_SELL_LIST;
                g_world_shop_fur_opened = 0;
            } else {
                thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF845, 0);
                g_world_shop_fur_waiting_message = 1;
            }
        } else {
            g_world_shop_menu_step = WORLD_SHOP_STEP_EXIT_FADE;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF831, 0);
            g_world_shop_fur_opened = 0;
        }
    }
    world_shop_run_obtain_gil_menu();
}

/* Query the selected poached-stock entry with a 0 quantity adjustment.
 *
 * An empty menu returns 0. */
s32 world_shop_get_selected_poached_item_quantity(s32 unused_row) {
    if (g_menu_list_entry_count != 0)
        return world_shop_adjust_poached_item_quantity(g_item_list_entries[g_menu_list_selected_index], 0);
    return 0;
}

/* Fur shop: the list of the party's own inventory to sell; sibling of
 * world_shop_run_fur_buy_list_step.
 *
 * The callback block is written in ascending address order (794 -> 7ac), as
 * the sibling writes it. That makes gcc materialise the two twice-used
 * addresses (count_2 in a0, count in v1) up front at 0xb0-0xbc and defer
 * their stores; the target's emission order then follows. First-use order,
 * not store order, decides this.
 */
void world_shop_run_fur_sell_list_step(void) {
    s32 message;

    if (g_world_shop_fur_sell_list_initialized == 0) {
        message = world_menu_has_items(0) != 0 ? 0xf840 : 0xf845;

        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, message, 0);
        g_world_shop_item_category = menu_init_cursor_position(WORLD_MENU_CURSOR_SHOP_ITEM_TAB, 0);
        world_item_build_category_list(-1, g_world_shop_item_category + 1,
            g_world_shop_item_category != 4 ? g_world_shop_item_category + 1 : ITEM_LIST_FILTER_OTHER,
            g_item_list_entries, ITEM_LIST_MODE_ALL);
        menu_init_scrollable_list_core_variant(
            g_item_list_entries, 0, g_text_section_pointers[TEXT_SECTION_ITEM_NAMES]);
        g_world_menu_script_callbacks[0].value = menu_get_entry_item_equipped_count;
        g_world_menu_script_callbacks[1].value = menu_get_entry_item_count;
        g_world_menu_script_callbacks[2].icon = menu_build_item_type_icon_source;
        g_world_menu_script_callbacks[3].icon = menu_get_entry_item_icon_source;
        g_world_menu_script_callbacks[4].value = world_item_get_selected_half_price;
        g_world_menu_script_callbacks[5].value = menu_get_entry_item_equipped_count;
        g_world_menu_script_callbacks[6].value = menu_get_entry_item_count;
        g_world_shop_fur_sell_list_initialized = 1;
    }

    menu_set_vsync_mode_and_event_speed(2);
    if ((g_menu_input_primary_repeat & PSX_PAD_CROSS) != 0) {
        g_world_shop_fur_sell_list_initialized = 0;
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf830, 0);
        g_world_shop_menu_step = WORLD_SHOP_STEP_FUR_MENU;
        g_sound_queued_effect_id = MAIN_SFX_CANCEL;
    } else if ((g_menu_input_primary_repeat & PSX_PAD_CIRCLE) != 0) {
        if (g_menu_list_entry_count == 0) {
            g_sound_queued_effect_id = MAIN_SFX_INVALID;
        } else {
            g_world_shop_menu_step = WORLD_SHOP_STEP_FUR_SELL_CONFIRM;
            g_world_shop_quantity_menu_data[1].selected_index = 0;
            g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
        }
    }
    world_menu_switch_item_category(g_menu_input_newly_pressed);
    menu_run_script_with_palette_mode_variant(
        &g_world_shop_sale_confirm_menu_script, g_menu_input_primary_repeat, g_menu_input_lock_mode);
    world_shop_run_obtain_gil_menu();
}

/* Fur shop: confirm a sale. The party receives half price, and the sold copies join the
 * poached stock. */
void world_shop_run_fur_sell_confirm_step(void) {
    s32 result = 1;

    if (g_world_shop_fur_sale_message_wait != 0) {
        if (g_menu_input_primary_repeat
            & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE | PSX_PAD_DPAD_MASK)) {
            g_world_shop_fur_sale_message_wait = 0;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF841, 0);
            g_world_shop_menu_step = WORLD_SHOP_STEP_FUR_SELL_LIST;
            g_world_menu_prompt_active = 0;
        }
    } else {
        result = world_shop_run_fur_sell_quantity_menu();
    }
    if (result == 0) {
        if (g_world_menu_selection_results[WORLD_MENU_SELECTION_PROMPT_RESULT] == 0) {
            world_shop_adjust_gil(world_item_get_half_price(g_item_list_entries[g_menu_list_selected_index])
                * g_world_menu_selection_results[WORLD_MENU_SELECTION_SHOP_QUANTITY]);
            item_adjust_inventory_count(g_item_list_entries[g_menu_list_selected_index],
                -g_world_menu_selection_results[WORLD_MENU_SELECTION_SHOP_QUANTITY]);
            world_shop_adjust_poached_item_quantity(g_item_list_entries[g_menu_list_selected_index],
                g_world_menu_selection_results[WORLD_MENU_SELECTION_SHOP_QUANTITY]);
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF837, 0);
            item_rebuild_category_list(g_world_shop_item_category);
            world_item_build_category_list(0, g_world_shop_item_category + 1,
                g_world_shop_item_category != 4 ? g_world_shop_item_category + 1 : ITEM_LIST_FILTER_OTHER,
                g_item_list_entries, ITEM_LIST_MODE_ALL);
            world_menu_init_and_load_scrollable_list(g_item_list_entries, g_menu_list_selected_index,
                g_text_section_pointers[TEXT_SECTION_ITEM_NAMES], &g_world_shop_sale_confirm_menu_script);
            menu_set_window_scale_step(10);
        } else {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF838, 0);
        }
    }
    if (result <= 0) {
        g_world_shop_fur_sale_message_wait = 1;
    }
    world_menu_switch_item_category(0);
    menu_run_script_with_palette_mode_variant(
        &g_world_shop_sale_confirm_menu_script, 0, thread_is_running(THREAD_SCREEN_SLOT_TEXT));
    world_shop_run_obtain_gil_menu();
}

/* Fur shop: the quantity prompt of a sale, capped by the inventory count. */
s32 world_shop_run_fur_sell_quantity_menu(void) {
    s32 count;

    if (g_world_shop_fur_quantity_menu_running == 0) {
        g_world_shop_sell_total_shown = 0;
        g_world_shop_sell_quantity_item = g_item_list_entries[g_menu_list_selected_index];
        g_world_menu_prompt_active = 1;
        count = item_adjust_inventory_count(g_world_shop_sell_quantity_item, 0);
        if (count == 0) {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF842, 0);
            return -1;
        }
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF843, 0);
        g_world_shop_quantity_limit = count;
        g_world_shop_sell_total_shown = 0;
        g_sound_suppressed = 1;
    }
    g_world_shop_fur_quantity_menu_running
        = menu_start_managed_thread(WORLD_THREAD_SLOT_UPPER_MENU, g_world_shop_quantity_menu_data);
    if (thread_is_running(WORLD_THREAD_SLOT_UPPER_MENU_CHILD) != 0 && g_world_shop_sell_total_shown == 0) {
        g_text_substitution_values[0] = world_item_get_half_price(g_world_shop_sell_quantity_item)
            * g_world_menu_selection_results[WORLD_MENU_SELECTION_SHOP_QUANTITY];
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF844, 0);
        g_world_shop_sell_total_shown = 1;
        g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
    }
    if (thread_is_running(WORLD_THREAD_SLOT_UPPER_MENU_CHILD) == 0 && g_world_shop_sell_total_shown != 0) {
        g_world_shop_sell_total_shown = 0;
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF843, 0);
    }
    if (g_world_shop_fur_quantity_menu_running == 0) {
        if (g_world_menu_selection_results[WORLD_MENU_SELECTION_PROMPT_RESULT] == 0) {
            g_sound_queued_effect_id = MAIN_SFX_SHOP_TRANSACTION;
        }
        if (g_world_menu_selection_results[WORLD_MENU_SELECTION_PROMPT_RESULT] == 1) {
            g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
        }
        g_sound_suppressed = 0;
    }
    return g_world_shop_fur_quantity_menu_running;
}
