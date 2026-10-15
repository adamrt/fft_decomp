/* World screen fade state, start, tiles and disable flag. */

#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/types.h"

s32 world_gfx_get_fade_state(void) {
    if (g_world_gfx_fade_disabled != 0) {
        return WORLD_FADE_STATE_NONE;
    }
    return g_world_gfx_fade_decreasing + (g_world_gfx_fade_increasing * WORLD_FADE_STATE_INCREASING);
}

/* Start the fade from intensity 240 down to zero (the screen fades in),
 * unless fades are disabled. */
/* Target 0x8012d454. */
void world_gfx_start_decreasing_fade(void) {
    if (g_world_gfx_fade_disabled == 0) {
        g_world_gfx_fade_decreasing = 1;
        g_world_gfx_fade_increasing = 0;
        g_world_gfx_fade_intensity = 240;
    }
}

/* Start the fade from intensity 0 up to full (the screen fades out), unless
 * fades are disabled. */
/* Target 0x8012d48c. */
void world_gfx_start_increasing_fade(void) {
    if (g_world_gfx_fade_disabled == 0) {
        g_world_gfx_fade_decreasing = 0;
        g_world_gfx_fade_increasing = 1;
        g_world_gfx_fade_intensity = 0;
    }
}

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

/* Target 0x8012d5bc; sibling of world_gfx_update_fade_in_tile. Steps the
 * subtractive fade level up (the screen darkens) and draws the full-screen
 * fade tile; returns 1 while the fade is still running, 0 once done or when
 * fading is suppressed. */
s32 world_gfx_update_fade_out_tile(void) {
    s32 running = 1;
    s32 unused[4]; /* target frame is 0x10 larger than the live locals need */

    if (g_world_gfx_fade_disabled != 0) {
        return 0;
    }
    if (g_world_gfx_fade_increasing != 0) {
        g_world_gfx_fade_intensity += menu_get_event_speed() * 0x10;
        if (g_world_gfx_fade_intensity >= 0x100) {
            running = 0;
            g_world_gfx_fade_increasing = 0;
            g_world_gfx_fade_intensity = 0xFF;
        }
        menu_gfx_enqueue_draw_mode_variant(0, 0, GetTPage(0, 2, 0x100, 0), 0, g_world_ot_length - 2);
        g_world_fade_tile_color.r = g_world_fade_tile_color.g = g_world_fade_tile_color.b = g_world_gfx_fade_intensity;
        gfx_enqueue_tile(&g_world_gfx_fade_tile_rect, &g_world_fade_tile_color, 1, g_world_ot_length - 1);
    }
    return running;
}

/* When set, the screen fade helpers at 0x8012d418-0x8012d6c8 all bail out
 * immediately, leaving g_world_gfx_fade_intensity at 0x801ca760 untouched. */
void world_gfx_set_fade_disabled(u8 disabled) {
    g_world_gfx_fade_disabled = disabled;
}
