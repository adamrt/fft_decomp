#include "fft/thread.h"
#include "psx/types.h"

/* Return the id of another running thread whose 32-bit current task value equals
   task_id, or 0 when none matches. Skips slot 0 and the caller's own thread. */
s32 thread_find_running_by_task(s32 task_id) {
    s32 thread_id;

    for (thread_id = 1; thread_id < VARIANT_THREAD_TABLE_SLOT_COUNT; thread_id++) {
        if (thread_id != g_thread_current_id && thread_is_running_asm_variant(thread_id) != 0
            && g_thread_contexts[thread_id].task_id == task_id) {
            return thread_id;
        }
    }
    return 0;
}
