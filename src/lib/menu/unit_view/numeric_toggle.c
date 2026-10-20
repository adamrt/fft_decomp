/* Starting or stopping the numeric panel thread. */

#include "fft/menu_list.h"
#include "fft/thread.h"
#include "fft/unit_panel.h"
#include "psx/types.h"

/* Starts thread 12 on unit_panel_numbers_thread (parameter
 * g_unit_panel_numeric_panel_params) when `enable` is set and the thread
 * is idle; stops it otherwise. */
void unit_panel_toggle_numbers_thread(s32 enable) {
    if (enable != 0) {
        if (thread_is_running(THREAD_SCREEN_SLOT_NUMERIC_PANEL) == 0) {
            thread_start(THREAD_SCREEN_SLOT_NUMERIC_PANEL, unit_panel_numbers_thread);
            thread_parameters_set(THREAD_SCREEN_SLOT_NUMERIC_PANEL, (s32)&g_unit_panel_numeric_panel_params, 0, 0);
        }
    } else {
        menu_request_thread_stop(THREAD_SCREEN_SLOT_NUMERIC_PANEL);
    }
}
