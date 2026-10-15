#include "fft/battle_map.h"
#include "psx/types.h"

/* Start a fade of both background gradient colors toward a color derived
 * from mode, with red/green/blue as 8-bit offsets in 16.16 fixed point.
 *
 * The per-color twin of map_blend_ambient_light_color (same modes 0..10).
 * Each grayscale case needs its own block-scoped gray: one function-scope
 * variable spans four blocks, becomes a global pseudo and swaps $v0/$v1 in
 * the division, which also defeats the target's cross-jumped case tails. */
void map_blend_background_gradient_color(s32 mode, s32 frame_duration, s32 red, s32 green, s32 blue) {
    map_color_transition_channels_t* channels;
    map_background_gradient_colors_t current;
    s32 target_red;
    s32 target_green;
    s32 target_blue;
    s32 base_red;
    s32 base_green;
    s32 base_blue;
    s32 i;

    channels = g_map_background_gradient_transition.channels;
    for (i = 0; i < 2; i++) {
        switch (mode) {
        case MAP_COLOR_MODE_CURRENT:
            target_red = (red << 16) + channels[i].red;
            target_green = (green << 16) + channels[i].green;
            target_blue = (blue << 16) + channels[i].blue;
            break;
        case MAP_COLOR_MODE_CURRENT_HALF:
            target_red = channels[i].red / 2 + (red << 16);
            target_green = channels[i].green / 2 + (green << 16);
            target_blue = channels[i].blue / 2 + (blue << 16);
            break;
        case MAP_COLOR_MODE_CURRENT_GRAY: {
            s32 gray = (channels[i].red * 2 + channels[i].green * 3 + channels[i].blue) / 6;
            target_red = gray + (red << 16);
            target_green = gray + (green << 16);
            target_blue = gray + (blue << 16);
            break;
        }
        case MAP_COLOR_MODE_CURRENT_GRAY_HALF: {
            s32 gray = (channels[i].red * 2 + channels[i].green * 3 + channels[i].blue) / 12;
            target_red = gray + (red << 16);
            target_green = gray + (green << 16);
            target_blue = gray + (blue << 16);
            break;
        }
        case MAP_COLOR_MODE_BASE:
        case MAP_COLOR_MODE_BASE_REPEAT:
            target_red = (g_map_background_gradient_transition.colors[i].r + red) << 16;
            target_green = (g_map_background_gradient_transition.colors[i].g + green) << 16;
            target_blue = (g_map_background_gradient_transition.colors[i].b + blue) << 16;
            break;
        case MAP_COLOR_MODE_BASE_HALF:
            target_red = (g_map_background_gradient_transition.colors[i].r << 15) + (red << 16);
            target_green = (g_map_background_gradient_transition.colors[i].g << 15) + (green << 16);
            target_blue = (g_map_background_gradient_transition.colors[i].b << 15) + (blue << 16);
            break;
        case MAP_COLOR_MODE_BASE_GRAY: {
            s32 gray;

            base_red = g_map_background_gradient_transition.colors[i].r << 16;
            base_green = g_map_background_gradient_transition.colors[i].g << 16;
            base_blue = g_map_background_gradient_transition.colors[i].b << 16;
            gray = (base_red * 2 + base_green * 3 + base_blue) / 6;
            target_red = gray + (red << 16);
            target_green = gray + (green << 16);
            target_blue = gray + (blue << 16);
            break;
        }
        case MAP_COLOR_MODE_BASE_GRAY_HALF: {
            s32 gray;

            base_red = g_map_background_gradient_transition.colors[i].r << 16;
            base_green = g_map_background_gradient_transition.colors[i].g << 16;
            base_blue = g_map_background_gradient_transition.colors[i].b << 16;
            gray = (base_red * 2 + base_green * 3 + base_blue) / 12;
            target_red = gray + (red << 16);
            target_green = gray + (green << 16);
            target_blue = gray + (blue << 16);
            break;
        }
        case MAP_COLOR_MODE_RESTORE:
            target_red = g_map_background_gradient_transition.colors[i].r << 16;
            target_green = g_map_background_gradient_transition.colors[i].g << 16;
            target_blue = g_map_background_gradient_transition.colors[i].b << 16;
            break;
        case MAP_COLOR_MODE_STOP_REPEAT:
            g_map_background_gradient_transition.repeat = MAP_COLOR_REPEAT_OFF;
            return;
        }

        target_red &= ~0xFFFF;
        target_green &= ~0xFFFF;
        target_blue &= ~0xFFFF;
        if (target_red > 0xFF0000) {
            target_red = 0xFF0000;
        }
        if (target_green > 0xFF0000) {
            target_green = 0xFF0000;
        }
        if (target_blue > 0xFF0000) {
            target_blue = 0xFF0000;
        }
        if (target_red < 0) {
            target_red = 0;
        }
        if (target_green < 0) {
            target_green = 0;
        }
        if (target_blue < 0) {
            target_blue = 0;
        }

        if (frame_duration != 0) {
            channels[i].red_step = (target_red - (channels[i].red & ~0xFFFF)) / (frame_duration * 8);
            channels[i].green_step = (target_green - (channels[i].green & ~0xFFFF)) / (frame_duration * 8);
            channels[i].blue_step = (target_blue - (channels[i].blue & ~0xFFFF)) / (frame_duration * 8);
        } else {
            channels[i].red = target_red;
            channels[i].green = target_green;
            channels[i].blue = target_blue;
        }
    }
    if (frame_duration != 0) {
        g_map_background_gradient_transition.active = 1;
        g_map_background_gradient_transition.phase = 0;
        g_map_background_gradient_transition.tick = 0;
        g_map_background_gradient_transition.period = frame_duration;
        if (mode == MAP_COLOR_MODE_BASE_REPEAT) {
            g_map_background_gradient_transition.repeat = MAP_COLOR_REPEAT_TINTED;
            g_map_background_gradient_transition.repeat_color.r = red;
            g_map_background_gradient_transition.repeat_color.g = green;
            g_map_background_gradient_transition.repeat_color.b = blue;
        }
    } else {
        g_map_background_gradient_transition.active = 0;
        current.colors[MAP_BACKGROUND_GRADIENT_TOP].r = channels[0].red / 0x10000;
        current.colors[MAP_BACKGROUND_GRADIENT_TOP].g = channels[0].green / 0x10000;
        current.colors[MAP_BACKGROUND_GRADIENT_TOP].b = channels[0].blue / 0x10000;
        current.colors[MAP_BACKGROUND_GRADIENT_BOTTOM].r = channels[1].red / 0x10000;
        current.colors[MAP_BACKGROUND_GRADIENT_BOTTOM].g = channels[1].green / 0x10000;
        current.colors[MAP_BACKGROUND_GRADIENT_BOTTOM].b = channels[1].blue / 0x10000;
        map_light_state_command(MAP_LIGHT_COMMAND_SET_BACKGROUND_GRADIENT, &current);
    }
}
