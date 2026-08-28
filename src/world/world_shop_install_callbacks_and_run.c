#include "fft/world.h"

void world_shop_install_callbacks_and_run(void) {
    g_world_menu_script_callbacks[0] = world_shop_obtain_gil;
    g_world_menu_script_callbacks[1] = world_shop_add_fitting_room_cost;
    g_world_menu_aux_callback = world_shop_get_gil_minus_fitting_room_cost;
    world_menu_run_script_with_palette_mode(
        g_world_shop_fitting_cost_menu_script, g_world_input_primary_repeat, world_menu_is_busy(0));
}
