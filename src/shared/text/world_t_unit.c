#include "fft/thread.h"
#include "fft/world.h"

/* Release a dialog thread that is waiting for text: its task becomes RESUME
 * and its second parameter 0xffff. */
void world_text_resume_printing(s32 thread_id) {
    thread_t* thread = &g_threads[thread_id];

    if (thread->task_id == THREAD_TASK_DIALOG_AWAIT_TEXT) {
        thread->task_id = THREAD_TASK_RESUME;
        thread->function_parameter_2 = 0xffff;
    }
}

/* Returns 1 while the text thread's task id is RESUME. */
s32 world_text_is_printing(s32 thread_id) {
    thread_t* thread = &g_threads[thread_id];

    return thread->task_id == THREAD_TASK_RESUME;
}

/* Idle thread body: yield to the next WORLD thread, forever. */
void world_thread_idle_yield_forever(void) {
    for (;;) {
        thread_yield();
    }
}

/* Park every idle thread slot from 5 to 8 on world_thread_idle_yield_forever. */
void world_thread_start_idle_5_to_8(void) {
    s32 thread_id;

    for (thread_id = 8; thread_id >= 5; thread_id--) {
        thread_t* thread = &g_threads[thread_id];

        if (thread->is_running == 0) {
            thread_start(thread_id, world_thread_idle_yield_forever);
        }
    }
}
