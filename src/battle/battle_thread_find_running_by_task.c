#include "fft/thread.h"
#include "psx/types.h"

/* Return the id of any OTHER live event thread whose current 32-bit task value
   equals task_id, or 0 when none matches. Skips the caller's own thread
   (g_thread_current_id) and slot 0. */
s32 battle_thread_find_running_by_task(s32 task_id) {
    s32 thread_id;

    for (thread_id = 1; thread_id < THREAD_SLOT_COUNT; thread_id++) {
        if (thread_id != g_thread_current_id && thread_is_running_asm_variant(thread_id) != 0
            && g_thread_contexts[thread_id].task_id == task_id) {
            return thread_id;
        }
    }
    return 0;
}
