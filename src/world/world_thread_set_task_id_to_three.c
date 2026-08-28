#include "fft/data.h"

void world_thread_set_task_id_to_three(s32 thread_id) {
    world_thread_t* thread = &g_world_threads[thread_id];

    thread->task_id = NATIVE_THREAD_TASK_STOP_REQUEST;
}
