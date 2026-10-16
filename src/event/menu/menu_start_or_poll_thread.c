/* Restarting a menu thread with new data, or polling the running thread's status. */

#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Restart the menu thread with the given data, running 20 input-update
 * frames, or poll the running thread's status into g_menu_managed_thread_state. */
void menu_start_or_poll_thread(s32 thread_id, menu_entry_t* descriptor) {
    s32 state[2];
    s32 i;

    if (g_menu_managed_thread_state == 0) {
        menu_stop_thread_and_wait(thread_id);
        g_menu_thread_menu_data = descriptor;
        thread_start(thread_id, descriptor->thread_entry);
        thread_set_parameters(thread_id, (s32)g_menu_thread_menu_data, 0, 0);
        for (i = 0; i < 20; i++) {
            menu_input_update_event_state_variant(state, 0, 0);
        }
        g_menu_managed_thread_state = 1;
    } else {
        g_menu_managed_thread_state = thread_is_running(thread_id);
    }
}
