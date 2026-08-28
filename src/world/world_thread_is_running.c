#include "fft/data.h"
#include "fft/world.h"

s32 world_thread_is_running(s32 thread_id) {
    return g_world_threads[thread_id].is_running;
}
