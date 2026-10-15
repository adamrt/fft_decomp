#include "fft/event_bunit.h"

/* Calculate a clamped intensity falloff from BUNIT's animated focal point.
 *
 * Vertical distance is doubled before the Euclidean distance calculation,
 * matching the menu renderer's non-square coordinate scale. `dx` also holds
 * the result. */
s32 bunit_gfx_calculate_distance_falloff(s32 x, s32 y, s32 base_value, s32 minimum) {
    s32 dx;
    s32 dy;

    dx = abs(g_bunit_gfx_falloff_focus_x - x);
    dx *= dx;
    dy = abs(g_bunit_gfx_falloff_focus_y - y);
    dy *= 2;
    dy *= dy;
    dx = base_value - (csqrt((dy + dx) * ONE) >> 12);
    if (dx < minimum) {
        dx = minimum;
    }
    return dx;
}
