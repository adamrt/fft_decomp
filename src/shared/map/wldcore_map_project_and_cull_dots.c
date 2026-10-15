#include "fft/script.h"
#include "fft/wldcore.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Projects every world-map dot and marks the ones that are off screen, hidden
 * by system flag 0x10, or disabled by their script variable with flag 0x10.
 *
 * The screen bounds are written as the target's unsigned range tests: x in
 * [-128, 128] and y in [-116, 120]. */
void wldcore_map_project_and_cull_dots(void) {
    VECTOR position;
    long flag;
    s32 i;

    for (i = 0; i < g_wldcore_map_dot_count; i++) {
        RotTrans(&g_wldcore_map_dots[i].position, &position, &flag);
        g_wldcore_map_dots[i].render.position.x = position.vx;
        g_wldcore_map_dots[i].render.position.y = position.vy;
        if (position.vx < -128 || position.vx > 128 || position.vy < -116 || position.vy > 120
            || (g_main_system_flags & MAIN_SYSTEM_FLAG_HIDE_MAP_DOTS)
            || script_get_variable(i + EVENT_SCRIPT_VAR_LOCATION_VISIBLE_FIRST) == 0) {
            g_wldcore_map_dots[i].render.flags |= WORLD_RENDER_RECORD_HIDDEN;
        } else {
            g_wldcore_map_dots[i].render.flags &= ~WORLD_RENDER_RECORD_HIDDEN;
        }
    }
}
