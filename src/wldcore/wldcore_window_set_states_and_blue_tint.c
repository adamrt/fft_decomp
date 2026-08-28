#include "fft/wldcore.h"
#include "psx/types.h"

/* Counterpart of 0x80076744: sets the two window states to 6 and 10, both
 * render record states to 2, and the render record colour to 0x40/0x50/0x70. */
void wldcore_window_set_states_and_blue_tint(wldcore_window_tint_request_t* param) {
    s32 pad[4];

    g_wldcore_window_records[param->window_a].palette = 6;
    g_wldcore_window_render_records[param->render_a].palette = 2;
    g_wldcore_window_render_records[param->render_b].palette = 2;
    g_wldcore_window_records[param->window_b].palette = 10;
    g_wldcore_window_render_records[param->render_color].red = 0x40;
    g_wldcore_window_render_records[param->render_color].green = 0x50;
    g_wldcore_window_render_records[param->render_color].blue = 0x70;
}
