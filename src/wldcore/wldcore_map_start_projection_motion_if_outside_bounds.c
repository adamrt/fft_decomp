#include "fft/wldcore.h"
#include "psx/gte.h"

enum {
    WLDCORE_PROJECTION_MOTION_ACTIVE = 0x01,
};

/* Start a wrapped projection move for a point outside the camera bounds.
 *
 * The motion record snapshots the current origin and stores the target
 * distance in the same 8-bit fractional scale used by its progress field. */
s32 wldcore_map_start_projection_motion_if_outside_bounds(const wldcore_point32_t* point, const s16* coordinates) {
    s32 dx;
    s32 dy;
    s32 dx_squared;
    /* Pin required: unpinned, dx*dx takes $a1 and dy*dy the csqrt argument
     * register $a0. */
    register s32 dy_squared __asm__("$3");

    if (wldcore_map_is_point_outside_projection_bounds(point) == 0) {
        return 0;
    }

    wldcore_map_negate_and_clamp_coordinates(coordinates, &g_wldcore_map_projection_motion.target);
    g_wldcore_map_projection_motion.flags |= WLDCORE_PROJECTION_MOTION_ACTIVE;
    g_wldcore_map_projection_motion.origin = g_wldcore_map_projection_origin;

    dx = g_wldcore_map_projection_motion.target.x - g_wldcore_map_projection_motion.origin.vx;
    dy = g_wldcore_map_projection_motion.target.y - g_wldcore_map_projection_motion.origin.vy;
    dx_squared = dx * dx;
    dy_squared = dy * dy;
    g_wldcore_map_projection_motion.progress = 0;
    g_wldcore_map_projection_motion.distance = csqrt(dx_squared + dy_squared) >> 8;
    return 1;
}
