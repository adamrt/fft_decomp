#include "fft/wldcore.h"

/* Negate and clamp a signed coordinate pair to the world-scroll limits. */
void wldcore_map_negate_and_clamp_coordinates(const s16* input, wldcore_map_clamped_point32_t* output) {
    s32 x;
    s32 y;

    x = -input[0];
    y = -input[1];
    if (x < WLDCORE_SCROLL_MIN_X) {
        x = WLDCORE_SCROLL_MIN_X;
    }
    if (x > WLDCORE_SCROLL_MAX_X) {
        x = WLDCORE_SCROLL_MAX_X;
    }
    if (y < WLDCORE_SCROLL_MIN_Y) {
        y = WLDCORE_SCROLL_MIN_Y;
    }
    if (y > WLDCORE_SCROLL_MAX_Y) {
        y = WLDCORE_SCROLL_MAX_Y;
    }
    output->x = x;
    output->y = y;
}
