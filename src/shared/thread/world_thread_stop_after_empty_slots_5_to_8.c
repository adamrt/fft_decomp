#include "fft/thread.h"
#include "fft/world.h"

/* For each idle thread slot from 8 down to 5, suspend the slot after it. */
void world_thread_stop_after_empty_slots_5_to_8(void) {
    s32 thread_id;

    for (thread_id = 8; thread_id >= 5; thread_id--) {
        thread_t* thread = &g_threads[thread_id];

        if (thread->is_running == 0) {
            thread_suspend(thread_id + 1);
        }
    }
}
