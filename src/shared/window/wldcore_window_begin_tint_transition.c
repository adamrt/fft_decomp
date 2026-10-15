#include "fft/wldcore.h"

/* Activates the proposition-category cursor and render record: the window takes
 * palette 10 and its render record palette 2, the window's three 32-bit scratch fields are
 * cleared, and the current menu level's list content render record is hidden
 * while the transition runs. */
void wldcore_window_begin_tint_transition(wldcore_menu_list_window_level_t* level) {
    g_wldcore_window_records[level->main_window].palette = 10;
    g_wldcore_window_render_records[level->side_window].palette = 2;
    g_wldcore_window_records[level->main_window].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_STILL;
    g_wldcore_window_records[level->main_window].frame_index = g_wldcore_window_records[level->main_window].anim_counter
        = 0;
    g_main_system_flags |= MAIN_SYSTEM_FLAG_HIDE_CONTEXT_VALUE;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 2]
                                        .list_window.content_render]
        .flags |= WORLD_RENDER_RECORD_HIDDEN;
}
