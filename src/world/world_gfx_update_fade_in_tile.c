#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Target 0x8012d4c0. Steps the subtractive fade level down and draws the
 * full-screen fade tile (abr 2, so the screen brightens as the level falls);
 * returns 1 while the fade is still running, 0 once done or when fading is
 * suppressed. */
s32 world_gfx_update_fade_in_tile(void) {
    s32 running = 1;
    u8 level;
    u8* rgb;

    if (g_world_gfx_fade_disabled != 0) {
        return 0;
    }
    if (g_world_gfx_fade_decreasing != 0) {
        g_world_gfx_fade_intensity -= world_gfx_get_vsync_mode_or_one() * 0x10;
        if (g_world_gfx_fade_intensity <= 0) {
            g_world_gfx_fade_decreasing = 0;
            running = 0;
        }
        world_gfx_add_draw_mode_primitive(0, 0, GetTPage(0, 2, 0x100, 0) & 0xFFFF, 0, g_world_ot_length - 2);
        rgb = &g_world_fade_tile_color.r;
        level = g_world_gfx_fade_intensity;
        g_world_fade_tile_color.b = level;
        g_world_fade_tile_color.g = level;
        *rgb = level;
        world_gfx_append_tile_to_otag(&g_world_gfx_fade_tile_rect, rgb, 1, g_world_ot_length - 1);
    }
    return running;
}
