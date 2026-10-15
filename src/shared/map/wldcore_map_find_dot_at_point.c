#include "fft/script.h"
#include "fft/wldcore.h"
#include "psx/types.h"

/* Pick the highest-priority visible map dot whose screen position contains the
 * given point, returned as a one-based index (0 when nothing is hit).
 *
 * The point is passed by value in a0/a1; the target reads both members back
 * from the incoming argument home slots. */
s32 wldcore_map_find_dot_at_point(point32_t point) {
    s32 best;
    s32 i;
    s32 hit;
    s32 value;

    best = 0xFFFFFF;
    hit = 0;
    for (i = 0; i < g_wldcore_map_dot_count; i++) {
        if (script_get_variable(i + EVENT_SCRIPT_VAR_LOCATION_VISIBLE_FIRST) == 0) {
            continue;
        }
        if ((g_wldcore_map_dots[i].render.flags & (WORLD_RENDER_RECORD_BLANK | WORLD_RENDER_RECORD_HIDDEN)) != 0) {
            continue;
        }
        value = g_wldcore_map_dots[i].render.position.x;
        if (point.x < value - 10 || value + 10 < point.x) {
            continue;
        }
        value = g_wldcore_map_dots[i].render.position.y;
        if (point.y < value - 14 || value + 6 < point.y) {
            continue;
        }
        value = g_wldcore_map_dots[i].render.priority;
        if (value < best) {
            hit = i + 1;
            best = value;
        }
    }
    return hit;
}
