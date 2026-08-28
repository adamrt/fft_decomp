#include "fft/wldcore.h"

/* Sibling of the resetter at 0x8007cd78. The record stores may alias `param`,
 * so they invalidate the cached parameter loads and reproduce the target's
 * reloads between stores. The two leading locals are
 * what hoist the first pair of loads above the first store, and `index`
 * shares one address computation between the last two.
 *
 * `scratch` is unreferenced; it reproduces the
 * target's 8-byte leaf frame, which saves no registers and holds no spills. */
void wldcore_window_clear_slot_state(s32* param) {
    s32 first = param[0];
    s32 second = param[1];
    s32 index;
    s32 scratch[2];

    g_wldcore_window_render_records[param[2]].palette = 0;
    g_wldcore_window_records[second].palette = 0;
    g_wldcore_window_records[first].palette = 0;
    g_wldcore_window_records[param[0]].sequence = 2;
    index = param[0];
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;
}
