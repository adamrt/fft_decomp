#include "fft/battle.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Return the id of any OTHER live event thread whose current task word
   equals task_id, or 0 when none matches. Skips the caller's own thread
   (g_battle_current_thread_id) and slot 0. */
s32 battle_thread_find_running_by_task(s32 task_id) {
    s32 thread_id;

    thread_id = 1;
    do {
        if (thread_id != g_battle_current_thread_id && battle_thread_is_running_8014cc94(thread_id) != 0
            && g_battle_thread_task_ids[thread_id][0] == task_id) {
            return thread_id;
        }
        thread_id += 1;
    } while (thread_id < 0x10);
    return 0;
}
