#include "fft/world.h"

/*
 * Run the menu's state-4/state-5 subflow through thread 2.
 *
 * State 0 closes the subflow and hands control back as state 6; otherwise
 * the updated state is retained for the next call. The menu role is unverified.
 */
void world_shop_run_sell_menu_subflow_step(void) {
    if (g_world_shop_sell_subflow_initialized == 0) {
        if (world_thread_is_running(2) != 0) {
            return;
        }
        world_thread_start(2, world_text_message_box_thread);
        g_world_formation_unit_browse_enabled = 0;
        g_world_formation_unit_banner_enabled = 0;
        world_formation_stop_menu_threads();
        g_world_formation_unit_cycle_mode = 0;
        g_world_shop_sell_subflow_step = 4;
        world_gfx_clear_sprite_slot(5);
        world_gfx_clear_sprite_slot(6);
        world_gfx_clear_sprite_slot(7);
        world_gfx_clear_sprite_slot(8);
        world_gfx_clear_sprite_slot(9);
        g_world_shop_sell_subflow_initialized = 1;
    }
    g_world_shop_menu_step = g_world_shop_sell_subflow_step;
    if (g_world_shop_sell_subflow_step == 4) {
        world_shop_run_item_sell_list_step();
        if (g_world_shop_sell_subflow_step != g_world_shop_menu_step) {
            world_menu_reset_selection_results();
        }
    } else if (g_world_shop_sell_subflow_step == 5) {
        world_shop_run_item_sell_confirm_step();
    }
    if (g_world_shop_menu_step == 0) {
        g_world_shop_menu_step = 6;
        g_world_formation_unit_cycle_mode = 2;
        g_world_shop_sell_subflow_initialized = 0;
        g_world_formation_unit_browse_enabled = 1;
        world_script_set_vsync_mode_and_event_speed(2);
        world_thread_set_parameters(2, 0x19, -1, 0);
        return;
    }
    g_world_shop_sell_subflow_step = g_world_shop_menu_step;
    g_world_shop_menu_step = 11;
}
