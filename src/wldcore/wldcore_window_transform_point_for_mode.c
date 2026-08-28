#include "fft/wldcore.h"

/* Transform a render position into the coordinate space selected by mode.
 *
 * The callee receives the adjacent position and dimensions registers as one
 * eight-byte aggregate. Dimensions are preserved by the ABI but unused here.
 */
void wldcore_window_transform_point_for_mode(s32 mode, wldcore_window_render_bounds16_t bounds, s32* output) {
    switch (mode) {
    case 0:
    case 1: {
        s32 x = (s16)bounds.position.x / 4;
        s32 base;

        /* Keeps the `mode * 64` shift out of the division fixup's branch delay
         * slot, which the target leaves as a nop. */
        __asm__ volatile("");
        base = mode * 64 + 0x180;
        output[0] = x + base;
        output[1] = (s16)bounds.position.y;
        break;
    }
    case 2:
        output[0] = ((s16)bounds.position.x / 4) + 0x240;
        output[1] = (s16)bounds.position.y + 0x100;
        break;
    }
}
