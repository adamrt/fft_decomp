#include "fft/wldcore.h"

/* Apply a (negated) scroll amount to a position clamped to [-116, 128] (the
 * same x range wldcore_map_negate_and_clamp_coordinates clamps to) and leave the unapplied
 * remainder in *amount. */
void wldcore_map_apply_clamped_horizontal_scroll(s32* amount, s32* position) {
    s32 delta;
    s32 old_position;
    s32 value;

    delta = -(*amount);
    (*amount) = delta;
    if (delta < 0) {
        old_position = (*position);
        value = old_position + delta;
        if (value < WLDCORE_SCROLL_MIN_X) {
            value = WLDCORE_SCROLL_MIN_X;
        }
        (*amount) = delta - (value - old_position);
        (*position) = value;
    }

    if ((*amount) > 0) {
        old_position = (*position);
        value = old_position + (*amount);
        if (value >= (WLDCORE_SCROLL_MAX_X + 1)) {
            value = WLDCORE_SCROLL_MAX_X;
        }
        (*amount) -= value - old_position;
        (*position) = value;
    }

    (*amount) = -(*amount);
}
