#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

/* Starts thread 12 on menu_run_numeric_editor_frame_thread (parameter
 * g_world_numeric_editor_thread_params) when `enable` is set and the thread
 * is idle; stops it otherwise. */
void world_menu_toggle_numeric_editor_thread(s32 enable) {
    if (enable != 0) {
        if (thread_is_running(THREAD_SCREEN_SLOT_NUMERIC_EDITOR) == 0) {
            thread_start(THREAD_SCREEN_SLOT_NUMERIC_EDITOR, menu_run_numeric_editor_frame_thread);
            thread_set_parameters(THREAD_SCREEN_SLOT_NUMERIC_EDITOR, (s32)&g_world_numeric_editor_thread_params, 0, 0);
        }
    } else {
        world_menu_stop_unit_status_banner_thread(THREAD_SCREEN_SLOT_NUMERIC_EDITOR);
    }
}
