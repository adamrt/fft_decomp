#include "fft/wldcore.h"
#include "psx/gte.h"
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
        g_wldcore_map_dots[i].screen_x = position.vx;
        g_wldcore_map_dots[i].screen_y = position.vy;
        if ((u32)(position.vx + 128) >= 257 || (u32)(position.vy + 116) >= 237 || (g_main_system_flags & 0x10)
            || world_script_get_variable(i + 0x200) == 0) {
            g_wldcore_map_dots[i].flags |= 0x10;
        } else {
            g_wldcore_map_dots[i].flags &= ~0x10;
        }
    }
}
