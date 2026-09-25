#include "fft/event_bunit.h"
#include "psx/types.h"

/* Start (enable=1) or stop (enable=0) sub-threads 9 and 12. Sister of
 * bunit_thread_toggle_7 / _8. */
void bunit_thread_toggle_9_and_12(s32 enable) {
    if (enable != 0) {
        if (g_bunit_status_display_flags & 0x60) {
            return;
        }
        if (battle_thread_is_running(9) != 0) {
            return;
        }
        battle_thread_start(9, bunit_panel_run_character_status_thread);
        battle_thread_set_parameters(9, &g_bunit_character_status_frame_config, 0, 0);
        battle_thread_start(0xC, bunit_run_numeric_editor_thread);
        battle_thread_set_parameters(0xC, &g_bunit_numeric_editor_thread_params, 0, 0);
    } else {
        bunit_thread_request_stop(9);
        bunit_thread_request_stop(0xC);
    }
}
