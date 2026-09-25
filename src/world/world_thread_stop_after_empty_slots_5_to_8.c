#include "fft/world.h"

void world_thread_stop_after_empty_slots_5_to_8(void) {
    s32 thread_id = 8;

    do {
        native_thread_t* thread = &g_world_threads[thread_id];

        if (thread->is_running == 0) {
            world_thread_suspend(thread_id + 1);
        }
        thread_id--;
    } while (thread_id >= 5);
}
