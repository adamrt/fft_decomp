#include "fft/world.h"
#include "psx/types.h"

void world_menu_toggle_unit_status_banner(s32 enable) {
    if (enable != 0) {
        if (world_thread_is_running(8) == 0) {
            world_thread_start(8, world_menu_unit_status_banner_thread);
            world_thread_set_parameters(8, (s32)&g_world_status_display_thread_params, 0, 0);
        }
    } else {
        world_menu_stop_unit_status_banner_thread(8);
    }
}
