#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "fft/main_runtime.h"
#include "psx/types.h"

/* Fade state shared with battle_gfx_update_screen_color_modulation_fade: the
 * final colour, the colour in effect, a Q8 accumulator and per-frame Q8 step
 * per channel, and the frames left. */

/*
 * Start a screen colour-modulation fade towards (red, green, blue).
 *
 * The duration is scaled by the animation speed; a fade that rounds to zero
 * frames only records the target colour. The overlay draw mode is rebuilt
 * with the given semi-transparency mode for both screen polarities.
 */
void battle_gfx_start_screen_color_modulation_fade(u16 blend_mode, u16 red, u16 green, u16 blue, u16 duration) {
    s16 frames;

    if (duration != 0) {
        frames = duration / g_animation_speed;
        g_battle_gfx_screen_color_modulation_target[0] = red;
        g_battle_gfx_screen_color_modulation_target[1] = green;
        g_battle_gfx_screen_color_modulation_target[2] = blue;
        g_battle_gfx_screen_color_modulation_fade_frames = frames;
        if (frames != 0) {
            g_battle_gfx_screen_color_modulation_value[0] = g_battle_gfx_screen_color_modulation[0] << 8;
            g_battle_gfx_screen_color_modulation_value[1] = g_battle_gfx_screen_color_modulation[1] << 8;
            g_battle_gfx_screen_color_modulation_value[2] = g_battle_gfx_screen_color_modulation[2] << 8;
            g_battle_gfx_screen_color_modulation_step[0]
                = ((g_battle_gfx_screen_color_modulation_target[0] - g_battle_gfx_screen_color_modulation[0]) << 8)
                / frames;
            g_battle_gfx_screen_color_modulation_step[1]
                = ((g_battle_gfx_screen_color_modulation_target[1] - g_battle_gfx_screen_color_modulation[1]) << 8)
                / frames;
            g_battle_gfx_screen_color_modulation_step[2]
                = ((g_battle_gfx_screen_color_modulation_target[2] - g_battle_gfx_screen_color_modulation[2]) << 8)
                / frames;
            SetDrawMode(&g_battle_gfx_screen_modulation_draw_modes[0], 0, 0, blend_mode << 5,
                &g_battle_gfx_screen_modulation_tex_window);
            g_battle_gfx_screen_modulation_draw_modes[1] = g_battle_gfx_screen_modulation_draw_modes[0];
        }
    }
}
