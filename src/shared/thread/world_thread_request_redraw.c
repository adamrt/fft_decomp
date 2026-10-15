#include "fft/thread.h"
#include "fft/world.h"

/* Set a thread's 32-bit task field 4, its redraw request. */
void world_thread_request_redraw(s32 thread_id) {
    g_threads[thread_id].task_words[4] = 1;
}
