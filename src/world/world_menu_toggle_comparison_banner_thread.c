#include "fft/world.h"
#include "psx/types.h"

/* Starts thread 7 on world_menu_unit_status_banner_thread (parameter g_world_comparison_display_thread_params) when
 * `flag` is set and the thread is idle; stops it otherwise. */
void world_menu_toggle_comparison_banner_thread(s32 flag) {
    if (flag != 0) {
        if (world_thread_is_running(7) == 0) {
            world_thread_start(7, world_menu_unit_status_banner_thread);
            world_thread_set_parameters(7, (s32)&g_world_comparison_display_thread_params, 0, 0);
        }
    } else {
        world_menu_stop_unit_status_banner_thread(7);
    }
}
