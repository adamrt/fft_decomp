#include "fft/world.h"
#include "psx/types.h"

/* Report whether a menu task or thread 6 is still running.
 *
 * Its only caller passes 0, but the body never reads that argument: `$a0` is
 * overwritten with 6 before any use and the result starts from zero. */
s32 world_menu_is_busy(s32 unused) {
    s32 result = 0;

    if (g_world_thread_task_active != 0) {
        result = 1;
    } else if (world_thread_is_running(6) != 0) {
        result = 1;
    }
    return result;
}
