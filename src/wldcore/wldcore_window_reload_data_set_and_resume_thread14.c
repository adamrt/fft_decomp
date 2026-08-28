#include "fft/wldcore.h"
#include "psx/types.h"

/* Clears the pending flags for the record selected by g_wldcore_active_saved_record.picture_render_index and, when
 * g_wldcore_active_saved_record.state_flags reports flag 0x40, for the two window records named by the
 * parameter, then resumes thread 0xe. */
void wldcore_window_reload_data_set_and_resume_thread14(s32* param) {
    s16 set = g_wldcore_active_saved_record.background_set;

    if (set != -1) {
        wldcore_load_data_set_into_scratch_buffer(set);
        wldcore_wait_for_file_load();
    }
    g_wldcore_window_render_records[g_wldcore_active_saved_record.picture_render_index].flags &= ~8;
    if (g_wldcore_active_saved_record.state_flags & 0x40) {
        g_wldcore_window_records[param[0]].flags &= ~0x10;
        g_wldcore_window_records[param[1]].flags &= ~0x10;
    }
    world_thread_resume(0xe);
}
