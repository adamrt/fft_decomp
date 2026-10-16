/* Whether a menu thread is running. */

#include "fft/menu_list.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Return the scene scheduler's running flag for a menu thread. */
s32 menu_thread_is_running(s32 thread_id) {
    return g_threads[thread_id].is_running;
}
