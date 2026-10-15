#include "fft/event_equip.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Starts thread 12 on equip_editor_run_numeric_thread (parameter
 * g_equip_numeric_panel_params) when `enable` is set and the thread is idle;
 * stops it otherwise. */
void equip_panel_toggle_numeric_editor_thread(s32 enable) {
    if (enable != 0) {
        if (thread_is_running(THREAD_SCREEN_SLOT_NUMERIC_EDITOR) == 0) {
            thread_start(THREAD_SCREEN_SLOT_NUMERIC_EDITOR, equip_editor_run_numeric_thread);
            thread_set_parameters(THREAD_SCREEN_SLOT_NUMERIC_EDITOR, (s32)g_equip_numeric_panel_params, 0, 0);
        }
    } else {
        menu_request_thread_stop(THREAD_SCREEN_SLOT_NUMERIC_EDITOR);
    }
}
