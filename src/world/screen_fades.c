/* Screen fade state, fade starts and per-tile fade updates. */

#include "fft/gfx.h"
#include "fft/gfx_primitive.h"
#include "fft/menu.h"
#include "fft/menu_gfx.h"
#include "fft/screen.h"
#include "psx/libgpu.h"
#include "psx/types.h"

extern s16 g_screen_fade_intensity;

/* Fade tile RGB channels, set to equal levels by the fade updaters. */
extern rgb24_t g_screen_fade_tile_color;

static u8 g_screen_fade_decreasing = 0;
static u8 g_screen_fade_disabled = 0;
static u8 g_screen_fade_increasing = 0;
static RECT g_screen_fade_tile_rect = { 0, 0, 256, 240 };

/* Return the active WORLD fades as SCREEN_FADE_STATE bits (decreasing 1, increasing 2), or none
 * while fades are disabled. */
s32 screen_fade_get_state(void) {
    if (g_screen_fade_disabled != 0) {
        return SCREEN_FADE_STATE_NONE;
    }
    return g_screen_fade_decreasing + (g_screen_fade_increasing * SCREEN_FADE_STATE_INCREASING);
}

/* Start the fade from intensity 240 down to zero (the screen fades in),
 * unless fades are disabled. */
void screen_fade_start_decreasing(void) {
    if (g_screen_fade_disabled == 0) {
        g_screen_fade_decreasing = 1;
        g_screen_fade_increasing = 0;
        g_screen_fade_intensity = 240;
    }
}

/* Start the fade from intensity 0 up to full (the screen fades out), unless
 * fades are disabled. */
void screen_fade_start_increasing(void) {
    if (g_screen_fade_disabled == 0) {
        g_screen_fade_decreasing = 0;
        g_screen_fade_increasing = 1;
        g_screen_fade_intensity = 0;
    }
}

/* Steps the subtractive fade level down by 16 per VSync of the frame step and
 * draws the full-screen fade tile (abr 2, so the screen brightens as the level
 * falls); returns 1 while the fade is still running, 0 once done or when
 * fading is suppressed.
 *
 * Unlike the fade out, the level is not clamped when it ends: at a frame step
 * of 2 it goes 16 to -16, and that last tile is drawn with the low byte, 240,
 * darkening the screen for one frame. */
s32 screen_fade_update_in(void) {
    s32 running = 1;

    if (g_screen_fade_disabled != 0) {
        return 0;
    }
    if (g_screen_fade_decreasing != 0) {
        g_screen_fade_intensity -= menu_get_event_speed() * 0x10;
        if (g_screen_fade_intensity <= 0) {
            g_screen_fade_decreasing = 0;
            running = 0;
        }
        menu_gfx_enqueue_draw_mode(0, 0, GetTPage(0, 2, 0x100, 0), 0, g_screen_ot_length - 2);
        g_screen_fade_tile_color.r = g_screen_fade_tile_color.g = g_screen_fade_tile_color.b = g_screen_fade_intensity;
        gfx_enqueue_tile(&g_screen_fade_tile_rect, &g_screen_fade_tile_color, 1, g_screen_ot_length - 1);
    }
    return running;
}

/* Sibling of screen_fade_update_in. Steps the subtractive fade level
 * up (the screen darkens) and draws the full-screen fade tile, ending clamped
 * at 0xFF; returns 1 while the fade is still running, 0 once done or when
 * fading is suppressed. */
s32 screen_fade_update_out(void) {
    s32 running = 1;
    s32 unused[4]; /* target frame is 0x10 larger than the live locals need */

    if (g_screen_fade_disabled != 0) {
        return 0;
    }
    if (g_screen_fade_increasing != 0) {
        g_screen_fade_intensity += menu_get_event_speed() * 0x10;
        if (g_screen_fade_intensity >= 0x100) {
            running = 0;
            g_screen_fade_increasing = 0;
            g_screen_fade_intensity = 0xFF;
        }
        menu_gfx_enqueue_draw_mode(0, 0, GetTPage(0, 2, 0x100, 0), 0, g_screen_ot_length - 2);
        g_screen_fade_tile_color.r = g_screen_fade_tile_color.g = g_screen_fade_tile_color.b = g_screen_fade_intensity;
        gfx_enqueue_tile(&g_screen_fade_tile_rect, &g_screen_fade_tile_color, 1, g_screen_ot_length - 1);
    }
    return running;
}

/* When set, the fade helpers in this file all bail out immediately, leaving
 * g_screen_fade_intensity untouched. */
void screen_fade_set_disabled(u8 disabled) {
    g_screen_fade_disabled = disabled;
}
