#include "fft/bunit.h"

/* Calculate a clamped intensity falloff from BUNIT's animated focal point.
 *
 * Vertical distance is doubled before the Euclidean distance calculation,
 * matching the menu renderer's non-square coordinate scale. */
s32 bunit_gfx_calculate_distance_falloff(s32 x, s32 y, s32 base_value, s32 minimum) {
    /* Pins kept: plain C swaps the $v0/$v1 roles of dx/|dx| and keeps the result in $s0. */
    register s32 dx __asm__("$2") = g_bunit_gfx_falloff_focus_x - x;
    register s32 ax __asm__("$3") = dx < 0 ? -dx : dx;
    s32 dy = g_bunit_gfx_falloff_focus_y - y;
    s32 ay = dy < 0 ? -dy : dy;
    register s32 squared_y __asm__("$2");
    register s32 sum __asm__("$4");
    register s32 dist __asm__("$3");

    ax *= ax;
    ay *= 2;
    squared_y = ay * ay;
    sum = squared_y + ax;
    dist = base_value - (csqrt(sum << 12) >> 12);
    if (dist < minimum) {
        dist = minimum;
    }
    return dist;
}
