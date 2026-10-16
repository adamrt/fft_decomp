#include "fft/menu.h"
#include "psx/types.h"

/* menu_start_managed_thread, first setting the entry's selected index when no menu thread is running.
 * Never called. */
s32 world_menu_run_thread_with_selected_index(s32 thread_id, menu_entry_t* arg, s16 value) {
    if (g_menu_managed_thread_state == 0) {
        arg->selected_index = value;
    }
    return menu_start_managed_thread(thread_id, arg);
}
