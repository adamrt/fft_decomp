#include "fft/battle.h"
#include "psx/types.h"

/* Pump event-thread execution until no other running thread has a task ID
   in 4..14, then commit script variable 0x27 as the newly loaded event id.
   Returns 1 when the event actually changed, 0 when it was already
   current. */
s32 battle_script_load_next_event(void) {
    s32 task_id;
    s32 event_id;

    do {
        battle_thread_yield();
        for (task_id = 4; task_id < 15; task_id++) {
            if (battle_thread_find_running_by_task(task_id) != 0) {
                break;
            }
        }
    } while (task_id != 15);

    event_id = battle_script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT);
    if (event_id == g_current_battle_event_id) {
        return 0;
    }
    battle_script_load_event(event_id);
    g_current_battle_event_id = event_id;
    return 1;
}
