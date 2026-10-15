#include "fft/battle_gfx.h"
#include "psx/types.h"

/* No retail code calls it. */
void battle_gfx_clear_screen_color_modulation(void) {
    g_battle_gfx_screen_color_modulation.b = 0;
    g_battle_gfx_screen_color_modulation.g = 0;
    g_battle_gfx_screen_color_modulation.r = 0;
}

/* Saves the screen color modulation for
 * battle_gfx_restore_screen_color_modulation. No retail code calls either. */
void battle_gfx_copy_screen_color_modulation(void) {
    g_battle_gfx_screen_color_modulation_backup.r = g_battle_gfx_screen_color_modulation.r;
    g_battle_gfx_screen_color_modulation_backup.g = g_battle_gfx_screen_color_modulation.g;
    g_battle_gfx_screen_color_modulation_backup.b = g_battle_gfx_screen_color_modulation.b;
}

/* No retail code calls it. */
void battle_gfx_restore_screen_color_modulation(void) {
    g_battle_gfx_screen_color_modulation.r = g_battle_gfx_screen_color_modulation_backup.r;
    g_battle_gfx_screen_color_modulation.g = g_battle_gfx_screen_color_modulation_backup.g;
    g_battle_gfx_screen_color_modulation.b = g_battle_gfx_screen_color_modulation_backup.b;
}

/* Fade state shared with battle_gfx_update_screen_color_modulation_fade: the
 * final color, the color in effect, a Q8 accumulator and per-frame Q8 step
 * per channel, and the frames left. */

/*
 * Start a screen color-modulation fade towards (red, green, blue).
 *
 * The duration is scaled by the animation speed; a fade that rounds to zero
 * frames only records the target color. The overlay draw mode is rebuilt
 * with the given semi-transparency mode for both screen polarities.
 */
void battle_gfx_start_screen_color_modulation_fade(u16 blend_mode, u16 red, u16 green, u16 blue, u16 duration) {
    s16 frames;

    if (duration != 0) {
        frames = duration / g_main_system_animation_speed;
        g_battle_gfx_screen_color_modulation_target.r = red;
        g_battle_gfx_screen_color_modulation_target.g = green;
        g_battle_gfx_screen_color_modulation_target.b = blue;
        g_battle_gfx_screen_color_modulation_fade_frames = frames;
        if (frames != 0) {
            g_battle_gfx_screen_color_modulation_value[0] = g_battle_gfx_screen_color_modulation.r << 8;
            g_battle_gfx_screen_color_modulation_value[1] = g_battle_gfx_screen_color_modulation.g << 8;
            g_battle_gfx_screen_color_modulation_value[2] = g_battle_gfx_screen_color_modulation.b << 8;
            g_battle_gfx_screen_color_modulation_step[0]
                = ((g_battle_gfx_screen_color_modulation_target.r - g_battle_gfx_screen_color_modulation.r) << 8)
                / frames;
            g_battle_gfx_screen_color_modulation_step[1]
                = ((g_battle_gfx_screen_color_modulation_target.g - g_battle_gfx_screen_color_modulation.g) << 8)
                / frames;
            g_battle_gfx_screen_color_modulation_step[2]
                = ((g_battle_gfx_screen_color_modulation_target.b - g_battle_gfx_screen_color_modulation.b) << 8)
                / frames;
            SetDrawMode(&g_battle_gfx_screen_modulation_draw_modes[0], 0, 0, blend_mode << 5,
                &g_battle_gfx_screen_modulation_tex_window);
            g_battle_gfx_screen_modulation_draw_modes[1] = g_battle_gfx_screen_modulation_draw_modes[0];
        }
    }
}

/* Color fade state at 0x800961c4..0x800961e1: the final color, the color
 * currently in effect, a Q8 accumulator per channel, the per-frame Q8 step per
 * channel, and the number of frames left.  The three-element shape is what
 * makes GCC hold the base addresses of the current-color and accumulator
 * arrays in registers for element 0. */

void battle_gfx_update_screen_color_modulation_fade(void) {
    u16 count;
    u16 remaining;
    u16 red;
    u16 green;
    u16 blue;

    count = g_battle_gfx_screen_color_modulation_fade_frames;
    if (count != 0) {
        remaining = count - 1;
        g_battle_gfx_screen_color_modulation_fade_frames = remaining;
        red = g_battle_gfx_screen_color_modulation_value[0] + g_battle_gfx_screen_color_modulation_step[0];
        g_battle_gfx_screen_color_modulation_value[0] = red;
        green = g_battle_gfx_screen_color_modulation_value[1] + g_battle_gfx_screen_color_modulation_step[1];
        g_battle_gfx_screen_color_modulation_value[1] = green;
        blue = g_battle_gfx_screen_color_modulation_value[2] + g_battle_gfx_screen_color_modulation_step[2];
        g_battle_gfx_screen_color_modulation_value[2] = blue;
        g_battle_gfx_screen_color_modulation.r = red >> 8;
        g_battle_gfx_screen_color_modulation.g = green >> 8;
        g_battle_gfx_screen_color_modulation.b = blue >> 8;
        if (remaining == 0) {
            g_battle_gfx_screen_color_modulation.r = g_battle_gfx_screen_color_modulation_target.r;
            g_battle_gfx_screen_color_modulation.g = g_battle_gfx_screen_color_modulation_target.g;
            g_battle_gfx_screen_color_modulation.b = g_battle_gfx_screen_color_modulation_target.b;
        }
    }
}

/* Queues the screen color-modulation overlay for this frame, the polygon and its
 * draw mode of the current screen polarity, unless all three components are 0.
 * `unused` reproduces the frame size. */
void battle_gfx_draw_screen_color_modulation_overlay(void) {
    char unused[24];

    if ((g_battle_gfx_screen_color_modulation.r | g_battle_gfx_screen_color_modulation.g
            | g_battle_gfx_screen_color_modulation.b)
        != 0) {
        g_battle_gfx_screen_modulation_polygons[g_main_gfx_screen_polarity].r0 = g_battle_gfx_screen_color_modulation.r;
        g_battle_gfx_screen_modulation_polygons[g_main_gfx_screen_polarity].g0 = g_battle_gfx_screen_color_modulation.g;
        g_battle_gfx_screen_modulation_polygons[g_main_gfx_screen_polarity].b0 = g_battle_gfx_screen_color_modulation.b;
        AddPrim(main_gfx_get_otag(), &g_battle_gfx_screen_modulation_polygons[g_main_gfx_screen_polarity]);
        AddPrim(main_gfx_get_otag(), &g_battle_gfx_screen_modulation_draw_modes[g_main_gfx_screen_polarity]);
    }
}
