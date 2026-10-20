/* Start, poll and stop menu threads through each screen's shared managed-thread state. */

#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Request stop, then clear this screen's managed-thread state. */
void menu_managed_thread_stop(s32 thread_id) {
    menu_request_thread_stop(thread_id);
    g_menu_managed_thread_state = 0;
}

#if !defined(FFT_EVENT_EQUIP)
/* CARD and WORLD also pump menu frames while waiting or restarting. */
/* Ask a thread to stop and run input-update frames until it has ended; then
 * clear the managed-thread state. */
void menu_thread_stop_and_wait(s32 id) {
    u32 otag;

    while (thread_is_running(id) != 0) {
        menu_request_thread_stop(id);
        menu_input_event_state_update(&otag, 0, 0);
    }
    g_menu_managed_thread_state = 0;
}
#endif

/* Start-or-poll helper for a menu thread: the first call starts `thread_id`
 * on the descriptor's thread function with the descriptor as parameter 1
 * (unless the slot is still busy) and returns 1; later calls return whether
 * the thread is still running, 0 once it has ended. */
s32 menu_managed_thread_start(s32 thread_id, menu_entry_t* descriptor) {
    if (g_menu_managed_thread_state == 0) {
        if (thread_is_running(thread_id) != 0) {
            return 1;
        }
        g_menu_thread_menu_data = descriptor;
        thread_start(thread_id, descriptor->thread_entry);
        thread_parameters_set(thread_id, (s32)g_menu_thread_menu_data, 0, 0);
        g_menu_managed_thread_state = 1;
        return 1;
    }
    g_menu_managed_thread_state = thread_is_running(thread_id);
    return g_menu_managed_thread_state;
}

#if defined(FFT_WORLD)
/* WORLD retains an uncalled selected-index start between its two start helpers. */
/* menu_managed_thread_start, first setting the entry's selected index when no menu thread is running.
 * Never called. */
s32 menu_managed_thread_start_at_selection(s32 thread_id, menu_entry_t* arg, s16 value) {
    if (g_menu_managed_thread_state == 0) {
        arg->selected_index = value;
    }
    return menu_managed_thread_start(thread_id, arg);
}
#endif

#if !defined(FFT_EVENT_EQUIP)
/* CARD and WORLD also pump menu frames while waiting or restarting. */
/* Restart the menu thread with the given data, running 20 input-update
 * frames, or poll the running thread's status into g_menu_managed_thread_state. */
void menu_start_or_poll_thread(s32 thread_id, menu_entry_t* descriptor) {
    u32 otag_storage[2];
    s32 i;

    if (g_menu_managed_thread_state == 0) {
        menu_thread_stop_and_wait(thread_id);
        g_menu_thread_menu_data = descriptor;
        thread_start(thread_id, descriptor->thread_entry);
        thread_parameters_set(thread_id, (s32)g_menu_thread_menu_data, 0, 0);
        for (i = 0; i < 20; i++) {
            menu_input_event_state_update(otag_storage, 0, 0);
        }
        g_menu_managed_thread_state = 1;
    } else {
        g_menu_managed_thread_state = thread_is_running(thread_id);
    }
}
#endif
