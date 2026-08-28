#include "fft/wldcore.h"

/* Routing both stores through one base variable (rather than
 * `&g_wldcore_window_record_positions[index]`) is what reproduces the target's separate base register
 * held in $a0 across the whole function. */
void wldcore_window_apply_layout(wldcore_window_layout_request_t* request) {
    wldcore_window_xy_t* xy = g_wldcore_window_record_positions;
    s32 index = request->window_index;
    s32 layout = request->layout;
    s32 kind;

    xy[index].x = g_wldcore_window_layout_rows[layout].x + 14;
    xy[index].y = g_wldcore_window_layout_rows[layout].y + 12;
    kind = g_wldcore_window_layout_rows[layout].kind;
    if (kind != request->cached_layout_kind) {
        request->cached_layout_kind = kind;
        g_wldcore_window_records[index].sequence = (kind != 0) ? 0x78 : 2;
        g_wldcore_window_records[index].anim_counter = 0;
        g_wldcore_window_records[index].frame_index = 0;
    }
}
