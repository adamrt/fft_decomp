#include "fft/battle.h"
#include "psx/types.h"

/* Start a fade of the ambient light toward a colour derived from mode, with
 * red/green/blue as 8-bit offsets applied in 16.16 fixed point.
 *
 * Modes 0..3 derive the base from the live transition channels, 4..9 from the
 * last applied colour, and mode 10 only clears the repeat record. Modes 2, 3,
 * 6 and 7 replace the base with the greyscale (2R + 3G + B) / 6, halved again
 * for 3 and 7. Mode 9 additionally records the offsets so the map animation
 * updater can replay them.
 *
 * There is no default case: modes 11 and above use the uninitialized target
 * colour, as the target does. */
void battle_map_blend_ambient_light_color(s32 mode, s32 frame_duration, s32 red, s32 green, s32 blue) {
    map_color_transition_channels_t* channels;
    map_color_t current;
    s32 target_red;
    s32 target_green;
    s32 target_blue;
    s32 grey;
    s32 base_red;
    s32 base_green;
    s32 base_blue;

    channels = &g_battle_map_ambient_light_transition.channels;
    switch (mode) {
    case 0:
        target_red = (red << 16) + channels->red;
        target_green = (green << 16) + channels->green;
        target_blue = (blue << 16) + channels->blue;
        break;
    case 1:
        target_red = channels->red / 2 + (red << 16);
        target_green = channels->green / 2 + (green << 16);
        target_blue = channels->blue / 2 + (blue << 16);
        break;
    case 2:
        grey = (channels->red * 2 + channels->green * 3 + channels->blue) / 6;
        target_red = grey + (red << 16);
        target_green = grey + (green << 16);
        target_blue = grey + (blue << 16);
        break;
    case 3:
        grey = (channels->red * 2 + channels->green * 3 + channels->blue) / 12;
        target_red = grey + (red << 16);
        target_green = grey + (green << 16);
        target_blue = grey + (blue << 16);
        break;
    case 4:
    case 9:
        target_red = (g_battle_map_ambient_light_color.red + red) << 16;
        target_green = (g_battle_map_ambient_light_color.green + green) << 16;
        target_blue = (g_battle_map_ambient_light_color.blue + blue) << 16;
        break;
    case 5:
        /* The base colour is unsigned, so the halved 16.16 value is a shift. */
        target_red = (g_battle_map_ambient_light_color.red << 15) + (red << 16);
        target_green = (g_battle_map_ambient_light_color.green << 15) + (green << 16);
        target_blue = (g_battle_map_ambient_light_color.blue << 15) + (blue << 16);
        break;
    case 6:
        base_red = g_battle_map_ambient_light_color.red << 16;
        base_green = g_battle_map_ambient_light_color.green << 16;
        base_blue = g_battle_map_ambient_light_color.blue << 16;
        grey = (base_red * 2 + base_green * 3 + base_blue) / 6;
        target_red = grey + (red << 16);
        target_green = grey + (green << 16);
        target_blue = grey + (blue << 16);
        break;
    case 7:
        base_red = g_battle_map_ambient_light_color.red << 16;
        base_green = g_battle_map_ambient_light_color.green << 16;
        base_blue = g_battle_map_ambient_light_color.blue << 16;
        grey = (base_red * 2 + base_green * 3 + base_blue) / 12;
        target_red = grey + (red << 16);
        target_green = grey + (green << 16);
        target_blue = grey + (blue << 16);
        break;
    case 8:
        target_red = g_battle_map_ambient_light_color.red << 16;
        target_green = g_battle_map_ambient_light_color.green << 16;
        target_blue = g_battle_map_ambient_light_color.blue << 16;
        break;
    case 10:
        g_battle_map_ambient_light_transition._unknown_04[0] = 0;
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
        channels->red_step = (target_red - (channels->red & ~0xFFFF)) / (frame_duration * 8);
        channels->green_step = (target_green - (channels->green & ~0xFFFF)) / (frame_duration * 8);
        channels->blue_step = (target_blue - (channels->blue & ~0xFFFF)) / (frame_duration * 8);
    } else {
        channels->red = target_red;
        channels->green = target_green;
        channels->blue = target_blue;
    }
    if (frame_duration != 0) {
        g_battle_map_ambient_light_transition.active = 1;
        g_battle_map_ambient_light_transition.phase = 0;
        g_battle_map_ambient_light_transition.tick = 0;
        g_battle_map_ambient_light_transition.period = frame_duration;
        if (mode == 9) {
            g_battle_map_ambient_light_transition._unknown_04[0] = 1;
            g_battle_map_ambient_light_transition._unknown_04[1] = red;
            g_battle_map_ambient_light_transition._unknown_04[2] = green;
            g_battle_map_ambient_light_transition._unknown_04[3] = blue;
        }
    } else {
        g_battle_map_ambient_light_transition.active = 0;
        current.red = channels->red / 0x10000;
        current.green = channels->green / 0x10000;
        current.blue = channels->blue / 0x10000;
        battle_map_light_state_command(MAP_LIGHT_COMMAND_SET_AMBIENT_COLOR, (u8*)&current);
    }
}
