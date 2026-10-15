#include "fft/wldcore.h"
#include "psx/types.h"

/* Restore the save confirmation windows and restart the cursor animation. */
void wldcore_window_reset_slot_state(wldcore_menu_save_confirm_level_t* level) {
    s32 index;

    g_wldcore_window_records[level->cursor_window].palette = 10;
    g_wldcore_window_records[level->second_window].palette = 6;
    g_wldcore_window_render_records[level->render_index].palette = 2;
    g_wldcore_window_records[level->cursor_window].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_STILL;
    index = level->cursor_window;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;
}
