#include "fft/wldcore.h"
#include "psx/gte.h"

/* Test the camera projection bounds while allowing wrap-edge exceptions.
 *
 * An off-screen point remains eligible when the current projection origin is
 * close enough to the opposite edge for wrapping to bring it into view. The
 * final `||` shares one `return 0` between both lower-bound tests, which keeps
 * GCC from folding the last test into a store-flag. */
s32 wldcore_map_is_point_outside_projection_bounds(const wldcore_point32_t* point) {
    s32 x;
    s32 y;

    x = point->x;
    if (x < -0x40 && 0x80 - g_wldcore_map_projection_origin.vx >= 0x21) {
        return 1;
    }
    if (x >= 0x49 && g_wldcore_map_projection_origin.vx + 0x74 >= 0x21) {
        return 1;
    }
    y = point->y;
    if (y < -0x40 && 0x50 - g_wldcore_map_projection_origin.vy >= 0x21) {
        return 1;
    }
    if (y < 0x41 || g_wldcore_map_projection_origin.vy + 0x40 < 0x21) {
        return 0;
    }
    return 1;
}
