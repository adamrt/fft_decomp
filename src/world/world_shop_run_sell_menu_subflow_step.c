#include "fft/thread.h"
#include "fft/world.h"

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
