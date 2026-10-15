#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/types.h"

/* Reload the saved background, restore the sound-novel windows when needed,
 * and resume the message thread. */
void wldcore_window_reload_data_set_and_resume_thread14(wldcore_menu_stack_record_t* level) {
    s16 set = g_wldcore_active_saved_record.background_set;

    if (set != -1) {
        wldcore_gfx_load_background_set(set);
        file_wait_for_pending_variant();
    }
    g_wldcore_window_render_records[g_wldcore_active_saved_record.picture_render_index].flags
        &= ~WORLD_RENDER_RECORD_BLANK;
    if (g_wldcore_active_saved_record.state_flags & WLDCORE_OPCODE_STATE_PARENT_WINDOWS_VISIBLE) {
        g_wldcore_window_records[level->sound_novel.left_window].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_records[level->sound_novel.right_window].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
    }
    thread_resume(WLDCORE_THREAD_SLOT_MESSAGE);
}
