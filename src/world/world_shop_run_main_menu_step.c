#include "fft/world.h"

void world_shop_run_main_menu_step(void) {
    u8 running;

    if (g_world_shop_main_menu_running == 0) {
        g_world_formation_unit_cycle_mode = 0;
        g_world_menu_cursor_position = 0;
    }
    running = world_menu_run_thread(6, &g_world_shop_main_menu_thread_data);
    g_world_shop_main_menu_running = running;
    if (running == 0) {
        world_gfx_clear_sprite_slots();
        if (g_world_menu_selection_results[0] == 0) {
            g_world_shop_menu_step = 1;
            g_world_shop_item_category = world_menu_set_cursor_position_2(0, 0);
        } else if (g_world_menu_selection_results[0] == 1) {
            g_world_shop_menu_step = 4;
        } else if (g_world_menu_selection_results[0] == 2) {
            g_world_shop_menu_step = 6;
            g_world_formation_selected_unit_index = 0;
            world_formation_stage_selected_unit();
            world_thread_set_parameters(2, 0x19, 0xF818, 0);
        } else {
            g_world_shop_menu_step = 0xD;
            world_thread_set_parameters(2, 0x19, 0xF803, 0);
        }
    }
    world_shop_run_obtain_gil_menu();
}
