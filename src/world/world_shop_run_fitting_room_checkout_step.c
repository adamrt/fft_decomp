#include "fft/world.h"
#include "psx/pad.h"
#include "psx/types.h"

void world_shop_run_fitting_room_checkout_step(void) {
    s32 result;

    g_world_shop_background_visible = 1;
    if (g_world_shop_checkout_complete != 0) {
        if (g_world_input_primary_repeat & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE)) {
            g_world_shop_menu_step = 6;
            world_thread_set_parameters(2, 0x19, -1, 0);
            g_world_shop_checkout_complete = 0;
            g_world_formation_unit_banner_enabled = 1;
            world_formation_recalculate_selected_unit_stats();
        }
    } else {
        result = world_shop_run_fitting_room_checkout_prompt();
        if (result == -1) {
            g_world_shop_menu_step = 0;
            world_thread_set_parameters(2, 0x19, 0xf802, 0);
            g_world_shop_fitting_room_initialized = 0;
        } else if (result == 0) {
            g_world_shop_checkout_complete = 1;
            world_thread_set_parameters(2, 0x19, 0xf818, 0);
        }
    }
    world_shop_run_obtain_gil_menu();
}
