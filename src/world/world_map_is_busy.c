#include "fft/world.h"
#include "psx/types.h"

/* Reports whether the world map is busy: a queued description text id, the
 * description text thread (15), the task thread flag, or the task thread (6). */
s32 world_map_is_busy(void) {
    s32 g_main_file_still_loading = 0;

    if (g_world_grid_menu_id != 0 || world_thread_is_running(15) != 0 || g_world_thread_task_active != 0
        || world_thread_is_running(6) != 0) {
        g_main_file_still_loading = 1;
    }
    return g_main_file_still_loading;
}
