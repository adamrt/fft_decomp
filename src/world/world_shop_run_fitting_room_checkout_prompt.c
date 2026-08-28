#include "fft/main_sound.h"
#include "fft/thread.h"
#include "fft/world.h"

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
        if (world_thread_is_running(2) != 0) {
            return 1;
        }
        g_world_shop_checkout_confirm_menu.selected_index = 0;
        g_world_shop_checkout_short_gil_menu.selected_index = 0;
        world_thread_start(2, world_text_message_box_thread);
        balance = world_shop_obtain_gil(0) - (cost = world_shop_add_fitting_room_cost(0));
        if (cost == 0) {
            world_shop_buy_from_fitting_room();
            return -1;
        }
        if (balance >= 0) {
            g_world_menu_sound_muted = 1;
            g_world_shop_checkout_prompt_menu = &g_world_shop_checkout_confirm_menu;
            g_world_text_substitution_values[0] = cost;
            world_thread_set_parameters(2, 0x19, 0xf819, 0);
            g_world_shop_checkout_insufficient_gil = 0;
        } else {
            g_world_shop_checkout_prompt_menu = &g_world_shop_checkout_short_gil_menu;
            world_thread_set_parameters(2, 0x19, 0xf81a, 0);
            g_world_shop_checkout_insufficient_gil = 1;
        }
        g_world_shop_checkout_prompt_initialized = 1;
        g_world_shop_checkout_prompt_active = 1;
    }
    if (g_world_shop_checkout_prompt_active != 0) {
        g_world_shop_checkout_prompt_active = world_menu_run_thread(0xf, g_world_shop_checkout_prompt_menu);
        if (g_world_shop_checkout_prompt_active != 0) {
            return 1;
        }
        g_world_shop_checkout_prompt_result = g_world_menu_selection_results[WORLD_MENU_SELECTION_PROMPT_RESULT];
        if (g_world_shop_checkout_insufficient_gil == 0) {
            g_world_menu_sound_muted = 0;
            if (g_world_shop_checkout_prompt_result == 0) {
                balance = -world_shop_add_fitting_room_cost(0);
                world_shop_obtain_gil(balance);
                world_shop_add_fitting_room_cost(balance);
                world_shop_buy_from_fitting_room();
                world_formation_save_records_to_party_data();
                g_world_menu_sound_effect_id = MAIN_SFX_SHOP_TRANSACTION;
                world_item_reconcile_sorted_list(0);
                world_item_reconcile_sorted_list(1);
                world_item_reconcile_sorted_list(2);
                world_item_reconcile_sorted_list(3);
                world_item_reconcile_sorted_list(4);
                g_world_shop_checkout_prompt_initialized = 0;
                return -1;
            }
            if (g_world_shop_checkout_prompt_result == 1) {
                g_world_menu_sound_effect_id = MAIN_SFX_CONFIRM;
            }
            world_thread_set_parameters(2, 0x19, 0xf81c, 0);
            return 1;
        }
        if (g_world_shop_checkout_prompt_result != 1) {
            world_shop_apply_fitting_room_items_to_all_units();
            g_world_shop_checkout_prompt_initialized = 0;
            return -1;
        }
        world_thread_set_parameters(2, 0x19, -1, 0);
        g_world_shop_checkout_prompt_initialized = 0;
        return 0;
    }
    if (world_menu_run_thread(0xf, &g_world_shop_checkout_short_gil_menu) != 0) {
        return 1;
    }
    g_world_shop_checkout_prompt_initialized = 0;
    if (g_world_menu_selection_results[WORLD_MENU_SELECTION_PROMPT_RESULT] != 1) {
        if (g_world_shop_checkout_prompt_result != 0) {
            world_shop_apply_fitting_room_items_to_all_units();
            return -1;
        }
        return -1;
    }
    world_thread_set_parameters(2, 0x19, -1, 0);
    return 0;
}
