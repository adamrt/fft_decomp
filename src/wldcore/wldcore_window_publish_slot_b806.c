#include "fft/wldcore.h"

/* Sibling of the resetter at 0x8007cd78, differing only in the value stored
 * into sequence. The record stores may alias `param`, so they invalidate
 * the cached parameter loads and reproduce the target's
 * reloads; `index` shares one address computation between the last two. */
void wldcore_window_publish_slot_b806(s32* param) {
    s32 index;

    g_wldcore_window_records[param[0]].palette = 10;
    g_wldcore_window_records[param[1]].palette = 6;
    g_wldcore_window_render_records[param[2]].palette = 2;
    g_wldcore_window_records[param[0]].sequence = 1;
    index = param[0];
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;
}
