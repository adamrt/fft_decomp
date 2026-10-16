/* Starting or stopping the numeric panel thread. */

#include "fft/menu_list.h"
#include "fft/menu_panel.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Starts thread 12 on menu_run_numeric_panel_frame_thread (parameter
 * g_menu_numeric_panel_params) when `enable` is set and the thread
 * is idle; stops it otherwise. */
void menu_toggle_numeric_panel_thread(s32 enable) {
    if (enable != 0) {
        if (thread_is_running(THREAD_SCREEN_SLOT_NUMERIC_PANEL) == 0) {
            thread_start(THREAD_SCREEN_SLOT_NUMERIC_PANEL, menu_run_numeric_panel_frame_thread);
            thread_set_parameters(THREAD_SCREEN_SLOT_NUMERIC_PANEL, (s32)&g_menu_numeric_panel_params, 0, 0);
        }
    } else {
        menu_request_thread_stop(THREAD_SCREEN_SLOT_NUMERIC_PANEL);
    }
}
