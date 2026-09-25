#include "fft/world.h"

void world_thread_request_redraw(s32 thread_id) {
    g_world_threads[thread_id].task_words[4] = 1;
}
