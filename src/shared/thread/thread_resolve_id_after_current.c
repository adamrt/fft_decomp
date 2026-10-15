#include "fft/thread.h"
#include "psx/types.h"

/* As thread_resolve_id, but the search for an idle slot starts above the
 * current thread's slot. */
s32 thread_resolve_id_after_current(s32 thread_id) {
    s32 index;

    if (thread_id < THREAD_RESET_SLOT_COUNT) {
        return thread_id;
    }

    index = g_thread_current_id + 1;
    if (index >= VARIANT_THREAD_TABLE_SLOT_COUNT) {
        thread_exit_current();
    } else {
        do {
            if (thread_is_running_asm_variant(index) == 0) {
                return index;
            }
            index++;
        } while (index < VARIANT_THREAD_TABLE_SLOT_COUNT);

        thread_exit_current();
    }
}
