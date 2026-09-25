#include "fft/world.h"

s32 world_text_is_printing(s32 thread_id) {
    native_thread_t* thread = &g_world_threads[thread_id];

    return thread->task_id == NATIVE_THREAD_TASK_RESUME;
}
