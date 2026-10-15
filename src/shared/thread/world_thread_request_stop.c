#include "fft/thread.h"
#include "fft/world.h"

/* Ask a thread to stop by setting its task id to
 * THREAD_TASK_STOP_REQUEST. */
void world_thread_request_stop(s32 thread_id) {
    thread_t* thread = &g_threads[thread_id];

    thread->task_id = THREAD_TASK_STOP_REQUEST;
}
