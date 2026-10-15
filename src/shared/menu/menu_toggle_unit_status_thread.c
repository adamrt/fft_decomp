#include "fft/event_bunit.h"
#include "fft/thread.h"
#include "psx/types.h"

/* BUNIT.OUT 001c3ee0 - Start (arg=1) or stop (arg=0) sub-thread 8, invoking
 * bunit_panel_run_unit_status_thread with parameter block g_bunit_status_display_thread_params. */
void menu_toggle_unit_status_thread(s32 enable) {
    if (enable != 0) {
        if (thread_is_running(THREAD_SCREEN_SLOT_UNIT_STATUS) == 0) {
            thread_start(THREAD_SCREEN_SLOT_UNIT_STATUS, menu_run_unit_status_thread_variant);
            thread_set_parameters(THREAD_SCREEN_SLOT_UNIT_STATUS, &g_menu_status_display_thread_params, 0, 0);
        }
    } else {
        menu_request_thread_stop(THREAD_SCREEN_SLOT_UNIT_STATUS);
    }
}
