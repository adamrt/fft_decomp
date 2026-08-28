#include "fft/bunit.h"
#include "psx/types.h"

/* BUNIT.OUT 001c3ee0 - Start (arg=1) or stop (arg=0) sub-thread 8, invoking
 * bunit_render_unit_status_panel_thread with parameter block g_bunit_status_display_thread_params. */
void bunit_thread_toggle_8(s32 enable) {
    if (enable != 0) {
        if (battle_thread_is_running(8) == 0) {
            battle_thread_start(8, bunit_render_unit_status_panel_thread);
            battle_thread_set_parameters(8, &g_bunit_status_display_thread_params, 0, 0);
        }
    } else {
        bunit_thread_request_stop(8);
    }
}
