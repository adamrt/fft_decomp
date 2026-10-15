#include "fft/thread.h"

s32 thread_get_current_task_id(void) {
    return g_thread_contexts[g_thread_current_id].task_id;
}
