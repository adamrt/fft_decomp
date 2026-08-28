#include "fft/wldcore.h"
#include "psx/types.h"

/* Clears the palette of two window records and two render records, resets one
 * render record's colour to neutral grey, then clears two more windows.
 *
 * The tied asm keeps the render-record base in a register (the target's
 * pointer-relative stores) instead of folding it into $at-indexed symbol
 * forms; the colour view re-bases that same register by +0x30. */
void wldcore_window_clear_states_and_restore_tint(wldcore_window_tint_request_t* param) {
    s32 pad[2];
    wldcore_window_render_record_t* records;
    wldcore_window_entry_52_rgb_t* colors;
    s32 window_b;
    s32 render_b;
    s32 render_a;
    s32 window_a;
    s32 window_d;
    s32 window_c;

    window_b = param->window_b;
    window_a = param->window_a;
    render_a = param->render_a;
    render_b = param->render_b;
    records = g_wldcore_window_render_records;
    __asm__("" : "=r"(records) : "0"(records));
    g_wldcore_window_records[window_b].palette = 0;
    records[render_b].palette = 0;
    records[render_a].palette = 0;
    g_wldcore_window_records[window_a].palette = 0;
    colors = (wldcore_window_entry_52_rgb_t*)&records->red;
    colors[param->render_color].red = 0x80;
    colors[param->render_color].green = 0x80;
    colors[param->render_color].blue = 0x80;
    window_d = param->window_d;
    window_c = param->window_c;
    g_wldcore_window_records[window_d].palette = 0;
    g_wldcore_window_records[window_c].palette = 0;
}
