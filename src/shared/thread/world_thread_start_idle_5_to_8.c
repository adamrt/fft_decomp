#include "fft/thread.h"
#include "fft/world.h"

/* Park every idle thread slot from 5 to 8 on world_thread_idle_yield_forever. */
void world_thread_start_idle_5_to_8(void) {
    s32 thread_id;

    for (thread_id = 8; thread_id >= 5; thread_id--) {
        thread_t* thread = &g_threads[thread_id];

        if (thread->is_running == 0) {
            thread_start(thread_id, world_thread_idle_yield_forever);
        }
    }
}
