#include "fft/wldcore.h"

/* Restore the active-proposition list's palettes and cursor animation. */
void wldcore_window_clear_layout_states(wldcore_menu_stack_record_t* level) {
    g_wldcore_window_records[level->proposition_list.cursor_window].palette
        = g_wldcore_window_records[level->proposition_list.title_window].palette
        = g_wldcore_window_records[level->proposition_list.count_window].palette
        = g_wldcore_window_records[level->proposition_list.days_window].palette
        = g_wldcore_window_render_records[level->proposition_list.render_index].palette = 0;
    g_wldcore_window_records[level->proposition_list.cursor_window].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_IDLE;
    g_wldcore_window_records[level->proposition_list.cursor_window].frame_index
        = g_wldcore_window_records[level->proposition_list.cursor_window].anim_counter = 0;
}
