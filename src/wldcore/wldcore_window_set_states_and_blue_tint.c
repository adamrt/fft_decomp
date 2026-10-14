#include "fft/wldcore.h"
#include "psx/types.h"

/* Counterpart of 0x80076744: sets the two window states to 6 and 10, both
 * render record states to 2, and the render record color to 0x40/0x50/0x70. */
void wldcore_window_set_states_and_blue_tint(wldcore_menu_send_unit_level_t* level) {
    s32 pad[4];

    g_wldcore_window_records[level->list_frame.window_index].palette = 6;
    g_wldcore_window_render_records[level->list_frame.render_index].palette = 2;
    g_wldcore_window_render_records[level->list_render].palette = 2;
    g_wldcore_window_records[level->cursor_window].palette = 10;
    g_wldcore_window_render_records[level->preview_render].color.r = 0x40;
    g_wldcore_window_render_records[level->preview_render].color.g = 0x50;
    g_wldcore_window_render_records[level->preview_render].color.b = 0x70;
}
