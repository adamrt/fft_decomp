/* Save and animate the tint overlay drawn over the battle scene. */

#include "fft/fade.h"
#include "fft/gfx.h"
#include "fft/system.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Color-tint fade state. */
static rgb24_t g_fade_tint;
extern rgb24_t g_fade_tint_backup;
extern rgb24_t g_fade_tint_target;
extern u16 g_fade_tint_value[3];
extern u16 g_fade_tint_step[3];
extern u16 g_fade_tint_fade_frames;
extern RECT g_fade_tint_tex_window;

/* No retail code calls it. */
void fade_tint_clear(void) {
    g_fade_tint.b = 0;
    g_fade_tint.g = 0;
    g_fade_tint.r = 0;
}

/* Saves the screen color modulation for
 * fade_tint_restore. No retail code calls either. */
void fade_tint_copy(void) {
    g_fade_tint_backup.r = g_fade_tint.r;
    g_fade_tint_backup.g = g_fade_tint.g;
    g_fade_tint_backup.b = g_fade_tint.b;
}

/* No retail code calls it. */
void fade_tint_restore(void) {
    g_fade_tint.r = g_fade_tint_backup.r;
    g_fade_tint.g = g_fade_tint_backup.g;
    g_fade_tint.b = g_fade_tint_backup.b;
}

/*
 * Start a screen color-modulation fade towards (red, green, blue).
 *
 * The duration is scaled by the vsync interval; a fade that rounds to zero
 * frames only records the target color, and a zero duration does nothing. The
 * overlay draw mode is rebuilt with the given semi-transparency mode for both
 * screen polarities.
 */
void fade_tint_fade_start(u16 blend_mode, u16 red, u16 green, u16 blue, u16 duration) {
    s16 frames;

    if (duration != 0) {
        frames = duration / g_system_vsync_interval;
        g_fade_tint_target.r = red;
        g_fade_tint_target.g = green;
        g_fade_tint_target.b = blue;
        g_fade_tint_fade_frames = frames;
        if (frames != 0) {
            g_fade_tint_value[0] = g_fade_tint.r << 8;
            g_fade_tint_value[1] = g_fade_tint.g << 8;
            g_fade_tint_value[2] = g_fade_tint.b << 8;
            g_fade_tint_step[0] = ((g_fade_tint_target.r - g_fade_tint.r) << 8) / frames;
            g_fade_tint_step[1] = ((g_fade_tint_target.g - g_fade_tint.g) << 8) / frames;
            g_fade_tint_step[2] = ((g_fade_tint_target.b - g_fade_tint.b) << 8) / frames;
            SetDrawMode(&g_fade_tint_draw_modes[0], 0, 0, blend_mode << 5, &g_fade_tint_tex_window);
            g_fade_tint_draw_modes[1] = g_fade_tint_draw_modes[0];
        }
    }
}

/* Advance the screen color-modulation fade by one frame: each channel's Q8
 * accumulator takes its step and the color in effect its integer part, and the
 * last frame lands exactly on the target color.
 *
 * The fade state (0x800961c4..0x800961e1) holds the target color, the color in
 * effect, the Q8 accumulator and per-frame Q8 step per channel, and the frames
 * left. The three-element array shape is what makes GCC hold the base
 * addresses of the current-color and accumulator arrays in registers for
 * element 0. */
void fade_tint_fade_update(void) {
    u16 count = g_fade_tint_fade_frames;
    u16 remaining;
    u16 red;
    u16 green;
    u16 blue;

    if (count != 0) {
        remaining = count - 1;
        g_fade_tint_fade_frames = remaining;
        red = g_fade_tint_value[0] + g_fade_tint_step[0];
        g_fade_tint_value[0] = red;
        green = g_fade_tint_value[1] + g_fade_tint_step[1];
        g_fade_tint_value[1] = green;
        blue = g_fade_tint_value[2] + g_fade_tint_step[2];
        g_fade_tint_value[2] = blue;
        g_fade_tint.r = red >> 8;
        g_fade_tint.g = green >> 8;
        g_fade_tint.b = blue >> 8;
        if (remaining == 0) {
            g_fade_tint.r = g_fade_tint_target.r;
            g_fade_tint.g = g_fade_tint_target.g;
            g_fade_tint.b = g_fade_tint_target.b;
        }
    }
}

/* Queues the screen color-modulation overlay for this frame, the polygon and its
 * draw mode of the current screen polarity, unless all three components are 0.
 * `unused` reproduces the frame size. */
void fade_tint_overlay_draw(void) {
    char unused[24];

    if ((g_fade_tint.r | g_fade_tint.g | g_fade_tint.b) != 0) {
        g_fade_tint_polygons[g_gfx_screen_polarity].r0 = g_fade_tint.r;
        g_fade_tint_polygons[g_gfx_screen_polarity].g0 = g_fade_tint.g;
        g_fade_tint_polygons[g_gfx_screen_polarity].b0 = g_fade_tint.b;
        AddPrim(gfx_battle_otag_get(), &g_fade_tint_polygons[g_gfx_screen_polarity]);
        AddPrim(gfx_battle_otag_get(), &g_fade_tint_draw_modes[g_gfx_screen_polarity]);
    }
}
