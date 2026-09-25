#include "fft/world.h"

void world_thread_wait_until_inactive(s32 thread_id) {
    do {
        world_thread_yield();
    } while (g_world_threads[thread_id].is_running != 0);
}
