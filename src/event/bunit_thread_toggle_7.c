#include "fft/event_bunit.h"
#include "psx/types.h"

/* BUNIT.OUT 001c3f44 - Start (arg=1) or stop (arg=0) sub-thread 7, invoking
 * bunit_render_unit_status_panel_thread with parameter block g_bunit_comparison_display_thread_params. Sister of
 * thread-8 toggle. */
void bunit_thread_toggle_7(s32 enable) {
    if (enable != 0) {
        if (battle_thread_is_running(7) == 0) {
            battle_thread_start(7, bunit_render_unit_status_panel_thread);
            battle_thread_set_parameters(7, &g_bunit_comparison_display_thread_params, 0, 0);
        }
    } else {
        bunit_thread_request_stop(7);
    }
}
