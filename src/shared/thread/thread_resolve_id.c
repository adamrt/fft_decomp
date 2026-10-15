#include "fft/thread.h"
#include "psx/types.h"

/* Returns the slot a thread should start in: a requested id below 16 as it is,
 * otherwise (THREAD_SLOT_FIRST_IDLE) the first idle slot from 1 up. With
 * every slot busy the calling thread exits, and the function has no return value
 * for that path. */
s32 thread_resolve_id(s32 requested_thread_id) {
    s32 i;

    if (requested_thread_id < THREAD_RESET_SLOT_COUNT) {
        return requested_thread_id;
    }
    i = 1;
    do {
        if (thread_is_running_asm_variant(i) == 0) {
            return i;
        }
        i++;
    } while (i < VARIANT_THREAD_TABLE_SLOT_COUNT);
    thread_exit_current();
}
