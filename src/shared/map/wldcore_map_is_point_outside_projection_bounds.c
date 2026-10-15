#include "fft/wldcore.h"
#include "psx/libgte.h"

/* Test the camera projection bounds while allowing wrap-edge exceptions.
 *
 * An off-screen point remains eligible when the current projection origin is
 * close enough to the opposite edge for wrapping to bring it into view. */
s32 wldcore_map_is_point_outside_projection_bounds(const point32_t* point) {
    if ((point->x < -0x40 && 0x80 - g_wldcore_map_projection_origin.vx >= 0x21)
        || (point->x >= 0x49 && g_wldcore_map_projection_origin.vx + 0x74 >= 0x21)) {
        return 1;
    }
    if ((point->y < -0x40 && 0x50 - g_wldcore_map_projection_origin.vy >= 0x21)
        || (point->y >= 0x41 && g_wldcore_map_projection_origin.vy + 0x40 >= 0x21)) {
        return 1;
    }
    return 0;
}
