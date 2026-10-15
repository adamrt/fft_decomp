#include "fft/thread.h"

/* Resolve a thread slot argument: a real slot id is returned unchanged; any
 * other value picks the first idle slot from 1 to 16. With none idle the
 * current thread exits. */
s32 world_thread_resolve_id(s32 thread_id) {
    s32 index;

    if (thread_id < THREAD_SLOT_COUNT) {
        return thread_id;
    }

    for (index = 1; index < 17; index++) {
        if (thread_is_running_asm_variant(index) == 0) {
            return index;
        }
    }

    thread_exit_current();
}
