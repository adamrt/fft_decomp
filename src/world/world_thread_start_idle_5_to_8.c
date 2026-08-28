#include "fft/data.h"
#include "fft/thread.h"
#include "fft/world.h"

void world_thread_start_idle_5_to_8(void) {
    s32 thread_id = 8;

    do {
        world_thread_t* thread = &g_world_threads[thread_id];

        if (thread->is_running == 0) {
            world_thread_start(thread_id, world_thread_idle_yield_forever);
        }
        thread_id--;
    } while (thread_id >= 5);
}
