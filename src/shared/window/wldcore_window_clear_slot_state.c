#include "fft/wldcore.h"

/* Restore the save confirmation palettes and cursor animation. */
void wldcore_window_clear_slot_state(wldcore_menu_stack_record_t* level) {
    s32 first = level->save_confirm.cursor_window;
    s32 second = level->save_confirm.second_window;
    s32 index;
    /* The original frame reserves eight unused bytes. */
    s32 scratch[2];

    g_wldcore_window_render_records[level->save_confirm.render_index].palette = 0;
    g_wldcore_window_records[second].palette = 0;
    g_wldcore_window_records[first].palette = 0;
    g_wldcore_window_records[level->save_confirm.cursor_window].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_IDLE;
    index = level->save_confirm.cursor_window;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;
}
