#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

/* Reports whether the world map is busy: a queued description text id, the
 * description text thread (15), the task thread flag, or the task thread (6). */
s32 world_map_is_busy(void) {
    s32 result = 0;

    if (g_world_grid_menu_id != MENU_GRID_HELP_NONE || thread_is_running(WORLD_THREAD_SLOT_UPPER_MENU) != 0
        || g_menu_input_lock_mode != 0 || thread_is_running(THREAD_SCREEN_SLOT_MENU) != 0) {
        result = 1;
    }
    return result;
}
