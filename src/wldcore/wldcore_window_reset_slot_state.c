#include "fft/wldcore.h"
#include "psx/types.h"

/* The record stores may alias `param`, so they invalidate the cached param
 * loads, which reproduces the target's reloads between stores. */
void wldcore_window_reset_slot_state(s32* param) {
    s32 index;

    g_wldcore_window_records[param[0]].palette = 10;
    g_wldcore_window_records[param[1]].palette = 6;
    g_wldcore_window_render_records[param[2]].palette = 2;
    g_wldcore_window_records[param[0]].sequence = 0;
    index = param[0];
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;
}
