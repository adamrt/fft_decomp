/* Screen fade state, fade starts and per-tile fade updates. */

#include "fft/gfx/fade.h"
#include "fft/gfx/gfx.h"
#include "fft/gfx/gfx_primitive.h"
#include "fft/menu/menu.h"
#include "fft/menu/menu_gfx.h"
#include "fft/menu/menu_screen.h"
#include "psx/libgpu.h"
#include "psx/types.h"

static s16 s_screen_fade_intensity;

/* Fade tile RGB channels, set to equal levels by the fade updaters. */
extern rgb24_t g_fade_menu_tile_color;

static u8 s_fade_in_active = 0;
static u8 s_fade_disabled = 0;
static u8 s_fade_out_active = 0;
static RECT s_screen_fade_tile_rect = { 0, 0, 256, 240 };

/* Return the active WORLD fades as SCREEN_FADE_STATE bits (decreasing 1, increasing 2), or none
 * while fades are disabled. */
s32 fade_menu_get_state(void) {
    if (s_fade_disabled) {
        return FADE_MENU_STATE_NONE;
    }
    return s_fade_in_active + s_fade_out_active * FADE_MENU_STATE_INCREASING;
}

/* Start the fade from intensity 240 down to zero (the screen fades in),
 * unless fades are disabled. */
void fade_menu_start_in(void) {
    if (!s_fade_disabled) {
        s_fade_in_active = 1;
        s_fade_out_active = 0;
        s_screen_fade_intensity = 240;
    }
}

/* Start the fade from intensity 0 up to full (the screen fades out), unless
 * fades are disabled. */
void fade_menu_start_out(void) {
    if (!s_fade_disabled) {
        s_fade_in_active = 0;
        s_fade_out_active = 1;
        s_screen_fade_intensity = 0;
    }
}

/* Steps the subtractive fade level down by 16 per VSync of the frame step and
 * draws the full-screen fade tile (abr 2, so the screen brightens as the level
 * falls). Returns 0 on the completion step or while disabled; otherwise 1,
 * including calls when this fade direction is inactive.
 *
 * Unlike the fade out, the level is not clamped when it ends: at a frame step
 * of 2 it goes 16 to -16, and that last tile is drawn with the low byte, 240,
 * darkening the screen for one frame. */
s32 fade_menu_update_in(void) {
    s32 running = 1;

    if (s_fade_disabled) {
        return 0;
    }
    if (s_fade_in_active) {
        s_screen_fade_intensity -= menu_get_event_speed() * 16;
        if (s_screen_fade_intensity <= 0) {
            s_fade_in_active = 0;
            running = 0;
        }
        menu_gfx_enqueue_draw_mode(0, 0, GetTPage(0, 2, 0x100, 0), 0, g_menu_screen_ot_length - 2);
        g_fade_menu_tile_color.r = g_fade_menu_tile_color.g = g_fade_menu_tile_color.b = s_screen_fade_intensity;
        gfx_enqueue_tile(&s_screen_fade_tile_rect, &g_fade_menu_tile_color, 1, g_menu_screen_ot_length - 1);
    }
    return running;
}

/* Sibling of fade_menu_update_in. Steps the subtractive fade level
 * up (the screen darkens) and draws the full-screen fade tile, ending clamped
 * at 255. Returns 0 on the completion step or while disabled; otherwise 1,
 * including calls when this fade direction is inactive. */
s32 fade_menu_update_out(void) {
    s32 running = 1;
    s32 unused[4]; /* target frame is 0x10 larger than the live locals need */

    if (s_fade_disabled) {
        return 0;
    }
    if (s_fade_out_active) {
        s_screen_fade_intensity += menu_get_event_speed() * 16;
        if (s_screen_fade_intensity >= 256) {
            running = 0;
            s_fade_out_active = 0;
            s_screen_fade_intensity = 255;
        }
        menu_gfx_enqueue_draw_mode(0, 0, GetTPage(0, 2, 0x100, 0), 0, g_menu_screen_ot_length - 2);
        g_fade_menu_tile_color.r = g_fade_menu_tile_color.g = g_fade_menu_tile_color.b = s_screen_fade_intensity;
        gfx_enqueue_tile(&s_screen_fade_tile_rect, &g_fade_menu_tile_color, 1, g_menu_screen_ot_length - 1);
    }
    return running;
}

/* When set, the fade helpers in this file all bail out immediately, leaving
 * s_screen_fade_intensity untouched. */
void fade_menu_set_disabled(u8 disabled) {
    s_fade_disabled = disabled;
}
