#include "fft/map.h"
#include "psx/types.h"

/* Start a fade of both background gradient colours toward a colour derived
 * from mode, with red/green/blue as 8-bit offsets in 16.16 fixed point.
 *
 * The per-colour twin of battle_map_blend_ambient_light_color (same modes 0..10).
 * Each greyscale case needs its own block-scoped grey: one function-scope
 * variable spans four blocks, becomes a global pseudo and swaps $v0/$v1 in
 * the division, which also defeats the target's cross-jumped case tails. */
void battle_map_blend_background_gradient_color(s32 mode, s32 frame_duration, s32 red, s32 green, s32 blue) {
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
        case 0:
            target_red = (red << 16) + channels[i].red;
            target_green = (green << 16) + channels[i].green;
            target_blue = (blue << 16) + channels[i].blue;
            break;
        case 1:
            target_red = channels[i].red / 2 + (red << 16);
            target_green = channels[i].green / 2 + (green << 16);
            target_blue = channels[i].blue / 2 + (blue << 16);
            break;
        case 2: {
            s32 grey = (channels[i].red * 2 + channels[i].green * 3 + channels[i].blue) / 6;
            target_red = grey + (red << 16);
            target_green = grey + (green << 16);
            target_blue = grey + (blue << 16);
            break;
        }
        case 3: {
            s32 grey = (channels[i].red * 2 + channels[i].green * 3 + channels[i].blue) / 12;
            target_red = grey + (red << 16);
            target_green = grey + (green << 16);
            target_blue = grey + (blue << 16);
            break;
        }
        case 4:
        case 9:
            target_red = (g_map_background_gradient_transition.colors[i].red + red) << 16;
            target_green = (g_map_background_gradient_transition.colors[i].green + green) << 16;
            target_blue = (g_map_background_gradient_transition.colors[i].blue + blue) << 16;
            break;
        case 5:
            target_red = (g_map_background_gradient_transition.colors[i].red << 15) + (red << 16);
            target_green = (g_map_background_gradient_transition.colors[i].green << 15) + (green << 16);
            target_blue = (g_map_background_gradient_transition.colors[i].blue << 15) + (blue << 16);
            break;
        case 6: {
            s32 grey;

            base_red = g_map_background_gradient_transition.colors[i].red << 16;
            base_green = g_map_background_gradient_transition.colors[i].green << 16;
            base_blue = g_map_background_gradient_transition.colors[i].blue << 16;
            grey = (base_red * 2 + base_green * 3 + base_blue) / 6;
            target_red = grey + (red << 16);
            target_green = grey + (green << 16);
            target_blue = grey + (blue << 16);
            break;
        }
        case 7: {
            s32 grey;

            base_red = g_map_background_gradient_transition.colors[i].red << 16;
            base_green = g_map_background_gradient_transition.colors[i].green << 16;
            base_blue = g_map_background_gradient_transition.colors[i].blue << 16;
            grey = (base_red * 2 + base_green * 3 + base_blue) / 12;
            target_red = grey + (red << 16);
            target_green = grey + (green << 16);
            target_blue = grey + (blue << 16);
            break;
        }
        case 8:
            target_red = g_map_background_gradient_transition.colors[i].red << 16;
            target_green = g_map_background_gradient_transition.colors[i].green << 16;
            target_blue = g_map_background_gradient_transition.colors[i].blue << 16;
            break;
        case 10:
            g_map_background_gradient_transition._unknown04[0] = 0;
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
        if (mode == 9) {
            g_map_background_gradient_transition._unknown04[0] = 1;
            g_map_background_gradient_transition._unknown04[1] = red;
            g_map_background_gradient_transition._unknown04[2] = green;
            g_map_background_gradient_transition._unknown04[3] = blue;
        }
    } else {
        g_map_background_gradient_transition.active = 0;
        current.first.red = channels[0].red / 0x10000;
        current.first.green = channels[0].green / 0x10000;
        current.first.blue = channels[0].blue / 0x10000;
        current.second.red = channels[1].red / 0x10000;
        current.second.green = channels[1].green / 0x10000;
        current.second.blue = channels[1].blue / 0x10000;
        battle_map_light_state_command(MAP_LIGHT_COMMAND_SET_BACKGROUND_GRADIENT, (u8*)&current);
    }
}
