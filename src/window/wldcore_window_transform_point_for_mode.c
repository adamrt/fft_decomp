#include "fft/wldcore.h"

/* Transform a render position into the coordinate space selected by mode.
 *
 * The callee receives the adjacent position and dimensions registers as one
 * eight-byte aggregate. Dimensions are preserved by the ABI but unused here.
 */
void wldcore_window_transform_point_for_mode(s32 mode, RECT bounds, point32_t* output) {
    switch (mode) {
    case 0:
    case 1:
        output->x = bounds.x / 4 + 0x180 + mode * 64;
        output->y = bounds.y;
        break;
    case 2:
        output->x = (bounds.x / 4) + 0x240;
        output->y = bounds.y + 0x100;
        break;
    }
}
