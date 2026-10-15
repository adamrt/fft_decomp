#include "fft/thread.h"
#include "fft/event_helpmenu.h"

/* The lowest running slot from THREAD_SLOT_SYSTEM_FUNCTION (4) to THREAD_SLOT_MENU (8),
 * or 9 when none runs. */
s32 thread_find_first_running_menu_thread(void) {
    s32 thread_id;

    for (thread_id = THREAD_SLOT_SYSTEM_FUNCTION; thread_id <= THREAD_SLOT_MENU; thread_id++) {
        if (g_threads[thread_id].is_running == 1) {
            return thread_id;
        }
    }
    return thread_id;
}
