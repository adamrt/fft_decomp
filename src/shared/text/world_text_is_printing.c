#include "fft/thread.h"
#include "fft/world.h"

/* Returns 1 while the text thread's task id is RESUME. */
s32 world_text_is_printing(s32 thread_id) {
    thread_t* thread = &g_threads[thread_id];

    return thread->task_id == THREAD_TASK_RESUME;
}
