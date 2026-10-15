#include "fft/battle.h"
#include "fft/battle_script.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Route controller input to the running battle event thread. */
void script_route_event_input_variant(void) {
    s32 thread;

    if (g_menu_input_mode != MENU_INPUT_TUTORIAL) {
        return;
    }
    thread = g_thread_current_id;
    if (thread == THREAD_SLOT_MAIN) {
        return;
    }
    if (g_script_event_input == 0) {
        g_script_saved_event_input = 0;
    }
    if (g_thread_contexts[thread].task_id == THREAD_TASK_RESUME) {
        g_script_event_input = g_script_unfiltered_controller_input;
    } else {
        g_script_event_input = g_script_saved_event_input;
    }
}
