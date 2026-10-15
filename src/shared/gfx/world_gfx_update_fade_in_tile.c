#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Target 0x8012d4c0. Steps the subtractive fade level down and draws the
 * full-screen fade tile (abr 2, so the screen brightens as the level falls);
 * returns 1 while the fade is still running, 0 once done or when fading is
 * suppressed. */
s32 world_gfx_update_fade_in_tile(void) {
    s32 running = 1;

    if (g_world_gfx_fade_disabled != 0) {
        return 0;
    }
    if (g_world_gfx_fade_decreasing != 0) {
        g_world_gfx_fade_intensity -= menu_get_event_speed() * 0x10;
        if (g_world_gfx_fade_intensity <= 0) {
            g_world_gfx_fade_decreasing = 0;
            running = 0;
        }
        menu_gfx_enqueue_draw_mode_variant(0, 0, GetTPage(0, 2, 0x100, 0), 0, g_world_ot_length - 2);
        g_world_fade_tile_color.r = g_world_fade_tile_color.g = g_world_fade_tile_color.b = g_world_gfx_fade_intensity;
        gfx_enqueue_tile(&g_world_gfx_fade_tile_rect, &g_world_fade_tile_color, 1, g_world_ot_length - 1);
    }
    return running;
}
