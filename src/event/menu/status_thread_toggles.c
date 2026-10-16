/* Unit status and comparison status thread toggles. */

#include "fft/menu_list.h"
#include "fft/menu_panel.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Open the unit status panel thread if it is not already running, or with enable 0 ask it to stop. */
void menu_toggle_unit_status_thread(s32 enable) {
    if (enable != 0) {
        if (thread_is_running(THREAD_SCREEN_SLOT_UNIT_STATUS) == 0) {
            thread_start(THREAD_SCREEN_SLOT_UNIT_STATUS, menu_run_unit_status_thread);
            thread_set_parameters(THREAD_SCREEN_SLOT_UNIT_STATUS, &g_menu_status_display_thread_params, 0, 0);
        }
    } else {
        menu_request_thread_stop(THREAD_SCREEN_SLOT_UNIT_STATUS);
    }
}

/* Open the comparison status panel thread (slot 7, menu_run_unit_status_thread on
 * g_menu_comparison_display_thread_params) if it is not already running, or with enable 0 ask it
 * to stop. */
void menu_toggle_comparison_status_thread(s32 enable) {
    if (enable != 0) {
        if (thread_is_running(THREAD_SCREEN_SLOT_COMPARISON_STATUS) == 0) {
            thread_start(THREAD_SCREEN_SLOT_COMPARISON_STATUS, menu_run_unit_status_thread);
            thread_set_parameters(THREAD_SCREEN_SLOT_COMPARISON_STATUS, &g_menu_comparison_display_thread_params, 0, 0);
        }
    } else {
        menu_request_thread_stop(THREAD_SCREEN_SLOT_COMPARISON_STATUS);
    }
}
