#include "fft/wldcore.h"
#include "psx/types.h"

/* Report the cursor step needed to walk the given point onto the map dot it
 * currently overlaps, clamped to +/-2 per axis. Returns the one-based dot
 * index, or 0 when the point hits nothing.
 *
 * The point is passed by value in a0/a1; the target reads both members back
 * from the incoming argument home slots. */
s32 wldcore_map_get_dot_snap_step(wldcore_point32_t point, s32* step_x, s32* step_y) {
    s32 hit;
    s32 dot_x;
    s32 dot_y;

    *step_x = 0;
    *step_y = 0;
    hit = wldcore_map_find_dot_at_point(point);
    if (hit != 0) {
        dot_x = g_wldcore_map_dots[hit - 1].screen_x;
        dot_y = g_wldcore_map_dots[hit - 1].screen_y - 4;
        if (dot_x < point.x) {
            if (dot_x > point.x - 2) {
                *step_x = dot_x - point.x;
            } else {
                *step_x = -2;
            }
        }
        if (point.x < dot_x) {
            if (dot_x < point.x + 2) {
                *step_x = dot_x - point.x;
            } else {
                *step_x = 2;
            }
        }
        if (dot_y < point.y) {
            if (dot_y > point.y - 2) {
                *step_y = dot_y - point.y;
            } else {
                *step_y = -2;
            }
        }
        if (point.y < dot_y) {
            if (dot_y < point.y + 2) {
                *step_y = dot_y - point.y;
            } else {
                *step_y = 2;
            }
        }
    }
    return hit;
}
