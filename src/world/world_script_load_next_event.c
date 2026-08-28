#include "fft/script_variables.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

/* Currently resident event id; the loader skips a reload when it is unchanged. */

/* Yield until no thread is running one of the event tasks 4..14, then load the
   event selected by script variable 0x27. Returns 1 when a load happened. */
s32 world_script_load_next_event(void) {
    s32 task_id;
    s32 event_id;

    do {
        world_thread_yield();
        for (task_id = 4; task_id < 15; task_id++) {
            if (world_thread_find_running_by_task(task_id) != 0) {
                break;
            }
        }
    } while (task_id != 15);

    event_id = world_script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT);
    if (event_id != g_world_script_loaded_event_id) {
        world_script_load_event(event_id);
        g_world_script_loaded_event_id = event_id;
        return 1;
    }
    return 0;
}
