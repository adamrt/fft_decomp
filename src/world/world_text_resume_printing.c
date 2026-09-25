#include "fft/data.h"

void world_text_resume_printing(s32 thread_id) {
    native_thread_t* thread = &g_world_threads[thread_id];

    if (thread->task_id == NATIVE_THREAD_TASK_DIALOG_AWAIT_TEXT) {
        thread->task_id = NATIVE_THREAD_TASK_RESUME;
        thread->function_parameter_2 = 0xffff;
    }
}
