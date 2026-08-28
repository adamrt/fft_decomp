#include "fft/battle.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Route controller input to the running battle event thread. */
void battle_script_route_event_input(void) {
    s32 thread;

    if (g_battle_menu_input_disabled != 2) {
        return;
    }
    thread = g_battle_current_thread_id;
    if (thread == 0) {
        return;
    }
    if (g_battle_script_event_input == 0) {
        g_battle_script_saved_event_input = 0;
    }
    if (g_battle_thread_task_ids[thread][0] == NATIVE_THREAD_TASK_RESUME) {
        g_battle_script_event_input = g_battle_script_unfiltered_controller_input;
    } else {
        g_battle_script_event_input = g_battle_script_saved_event_input;
    }
}
