#include "fft/world.h"
#include "psx/types.h"

/* Built at -O1, like its neighbours world_menu_toggle_thread_9 /
 * world_menu_toggle_thread_10_alt. */

void world_menu_toggle_preview_stats_window(s32 flag) {
    s32 stored;

    stored = flag;
    if (stored != 0) {
        stored = 1;
        if (!(g_world_status_display_thread_params.flags & 0x60) && world_thread_is_running(0xB) == 0) {
            world_thread_start(0xB, world_menu_run_numeric_editor_thread);
            world_thread_set_parameters(0xB, (s32)&g_world_preview_stats_thread_params, 0, 0);
        }
    } else {
        world_menu_stop_unit_status_banner_thread(0xB);
    }
    g_world_numeric_editor_thread_params.style = stored;
    g_world_preview_stats_window_active = stored;
}
