#include "fft/menu.h"
#include "fft/thread.h"

/* Start-or-poll helper for a menu thread: the first call starts `thread_id`
 * on the descriptor's thread function with the descriptor as parameter 1
 * (unless the slot is still busy) and returns 1; later calls return whether
 * the thread is still running, 0 once it has ended. */
s32 menu_start_managed_thread(s32 thread_id, menu_entry_t* descriptor) {
    if (g_menu_managed_thread_state == 0) {
        if (thread_is_running(thread_id) != 0) {
            return 1;
        }
        g_menu_thread_menu_data = descriptor;
        thread_start(thread_id, descriptor->thread_entry);
        thread_set_parameters(thread_id, (s32)g_menu_thread_menu_data, 0, 0);
        g_menu_managed_thread_state = 1;
        return 1;
    }
    g_menu_managed_thread_state = thread_is_running(thread_id);
    return g_menu_managed_thread_state;
}
