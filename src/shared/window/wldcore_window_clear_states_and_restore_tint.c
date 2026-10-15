#include "fft/wldcore.h"

/* Clears the palette of two window records and two render records, resets one
 * render record's color to neutral gray, then clears two more windows. */
void wldcore_window_clear_states_and_restore_tint(wldcore_menu_send_unit_level_t* level) {
    CVECTOR* color;

    g_wldcore_window_records[level->list_frame.window_index].palette
        = g_wldcore_window_render_records[level->list_frame.render_index].palette
        = g_wldcore_window_render_records[level->list_render].palette
        = g_wldcore_window_records[level->cursor_window].palette = 0;
    /* Each channel store reloads the level's render index. */
    color = &g_wldcore_window_render_records[level->preview_render].color;
    color->r = 0x80;
    color = &g_wldcore_window_render_records[level->preview_render].color;
    color->g = 0x80;
    color = &g_wldcore_window_render_records[level->preview_render].color;
    color->b = 0x80;
    g_wldcore_window_records[level->left_arrow].palette = g_wldcore_window_records[level->right_arrow].palette = 0;
}
