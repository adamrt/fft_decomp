#include "fft/world.h"
#include "psx/types.h"

/* Return the id of another running thread whose current task word equals
   task_id, or 0 when none matches. Skips slot 0 and the caller's own thread. */
s32 world_thread_find_running_by_task(s32 task_id) {
    s32 thread_id;

    thread_id = 1;
    do {
        if (thread_id != g_world_thread_current_id && world_thread_is_running_80100164(thread_id) != 0
            && g_world_thread_contexts[thread_id].task_id == task_id) {
            return thread_id;
        }
        thread_id += 1;
    } while (thread_id < 17);
    return 0;
}
