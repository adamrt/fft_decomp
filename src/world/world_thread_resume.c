#include "fft/world.h"

void world_thread_resume(s32 thread_id) {
    g_world_threads[thread_id].is_running = 1;
}
