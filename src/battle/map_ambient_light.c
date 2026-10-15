/* Map ambient light. Its start is a cut chosen by topic; its end is a cut chosen by topic. */

#include "fft/battle_map.h"
#include "psx/types.h"

/* Start a fade of the ambient light toward a color derived from mode, with
 * red/green/blue as 8-bit offsets applied in 16.16 fixed point.
 *
 * Modes 0..3 derive the base from the live transition channels, 4..9 from the
 * last applied color, and mode 10 only clears the repeat record. Modes 2, 3,
 * 6 and 7 replace the base with the grayscale (2R + 3G + B) / 6, halved again
 * for 3 and 7. Mode 9 additionally records the offsets so the map animation
 * updater can replay them.
 *
 * There is no default case: modes 11 and above use the uninitialized target
 * color, as the target does. */
void map_blend_ambient_light_color(s32 mode, s32 frame_duration, s32 red, s32 green, s32 blue) {
    map_color_transition_channels_t* channels;
    CVECTOR current;
    s32 target_red;
    s32 target_green;
    s32 target_blue;
    s32 gray;
    s32 base_red;
    s32 base_green;
    s32 base_blue;

    channels = &g_map_ambient_light_transition.channels;
    switch (mode) {
    case MAP_COLOR_MODE_CURRENT:
        target_red = (red << 16) + channels->red;
        target_green = (green << 16) + channels->green;
        target_blue = (blue << 16) + channels->blue;
        break;
    case MAP_COLOR_MODE_CURRENT_HALF:
        target_red = channels->red / 2 + (red << 16);
        target_green = channels->green / 2 + (green << 16);
        target_blue = channels->blue / 2 + (blue << 16);
        break;
    case MAP_COLOR_MODE_CURRENT_GRAY:
        gray = (channels->red * 2 + channels->green * 3 + channels->blue) / 6;
        target_red = gray + (red << 16);
        target_green = gray + (green << 16);
        target_blue = gray + (blue << 16);
        break;
    case MAP_COLOR_MODE_CURRENT_GRAY_HALF:
        gray = (channels->red * 2 + channels->green * 3 + channels->blue) / 12;
        target_red = gray + (red << 16);
        target_green = gray + (green << 16);
        target_blue = gray + (blue << 16);
        break;
    case MAP_COLOR_MODE_BASE:
    case MAP_COLOR_MODE_BASE_REPEAT:
        target_red = (g_map_ambient_light_color.r + red) << 16;
        target_green = (g_map_ambient_light_color.g + green) << 16;
        target_blue = (g_map_ambient_light_color.b + blue) << 16;
        break;
    case MAP_COLOR_MODE_BASE_HALF:
        /* The base color is unsigned, so the halved 16.16 value is a shift. */
        target_red = (g_map_ambient_light_color.r << 15) + (red << 16);
        target_green = (g_map_ambient_light_color.g << 15) + (green << 16);
        target_blue = (g_map_ambient_light_color.b << 15) + (blue << 16);
        break;
    case MAP_COLOR_MODE_BASE_GRAY:
        base_red = g_map_ambient_light_color.r << 16;
        base_green = g_map_ambient_light_color.g << 16;
        base_blue = g_map_ambient_light_color.b << 16;
        gray = (base_red * 2 + base_green * 3 + base_blue) / 6;
        target_red = gray + (red << 16);
        target_green = gray + (green << 16);
        target_blue = gray + (blue << 16);
        break;
    case MAP_COLOR_MODE_BASE_GRAY_HALF:
        base_red = g_map_ambient_light_color.r << 16;
        base_green = g_map_ambient_light_color.g << 16;
        base_blue = g_map_ambient_light_color.b << 16;
        gray = (base_red * 2 + base_green * 3 + base_blue) / 12;
        target_red = gray + (red << 16);
        target_green = gray + (green << 16);
        target_blue = gray + (blue << 16);
        break;
    case MAP_COLOR_MODE_RESTORE:
        target_red = g_map_ambient_light_color.r << 16;
        target_green = g_map_ambient_light_color.g << 16;
        target_blue = g_map_ambient_light_color.b << 16;
        break;
    case MAP_COLOR_MODE_STOP_REPEAT:
        g_map_ambient_light_transition.repeat = MAP_COLOR_REPEAT_OFF;
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
        g_map_ambient_light_transition.active = 1;
        g_map_ambient_light_transition.phase = 0;
        g_map_ambient_light_transition.tick = 0;
        g_map_ambient_light_transition.period = frame_duration;
        if (mode == MAP_COLOR_MODE_BASE_REPEAT) {
            g_map_ambient_light_transition.repeat = MAP_COLOR_REPEAT_TINTED;
            g_map_ambient_light_transition.repeat_color.r = red;
            g_map_ambient_light_transition.repeat_color.g = green;
            g_map_ambient_light_transition.repeat_color.b = blue;
        }
    } else {
        g_map_ambient_light_transition.active = 0;
        current.r = channels->red / 0x10000;
        current.g = channels->green / 0x10000;
        current.b = channels->blue / 0x10000;
        map_light_state_command(MAP_LIGHT_COMMAND_SET_AMBIENT_COLOR, &current);
    }
}

/* Start a fade of the map darkness toward color over frame_duration * 8
 * frames, or apply it at once when frame_duration is zero. */
void map_transition_darkness_color(s32 frame_duration, const CVECTOR* color) {
    map_color_transition_channels_t* channels;
    CVECTOR current;
    s32 red;
    s32 green;
    s32 blue;
    s32 target_red;
    s32 target_green;
    s32 target_blue;

    channels = &g_map_darkness_transition.channels;
    /* The separate target copies reproduce the target's register copies that
     * feed the immediate-store path. */
    red = color->r << 16;
    target_red = red;
    green = color->g << 16;
    target_green = green;
    blue = color->b << 16;
    target_blue = blue;
    if (frame_duration != 0) {
        g_map_darkness_transition.channels.red_step = (red - (channels->red & ~0xFFFF)) / (frame_duration * 8);
        g_map_darkness_transition.channels.green_step = (green - (channels->green & ~0xFFFF)) / (frame_duration * 8);
        g_map_darkness_transition.channels.blue_step = (blue - (channels->blue & ~0xFFFF)) / (frame_duration * 8);
    } else {
        channels->red = target_red;
        channels->green = target_green;
        channels->blue = target_blue;
    }
    if (frame_duration != 0) {
        g_map_darkness_transition.active = 1;
        g_map_darkness_transition.phase = 0;
        g_map_darkness_transition.tick = 0;
        g_map_darkness_transition.period = frame_duration;
    } else {
        g_map_darkness_transition.active = 0;
        current.r = channels->red / 0x10000;
        current.g = channels->green / 0x10000;
        current.b = channels->blue / 0x10000;
        map_light_state_command(MAP_LIGHT_COMMAND_SET_DARKNESS_COLOR, &current);
    }
}

/* Fade the map darkness to `color` and make it the base color that the
 * MAP_COLOR_MODE_BASE modes and RESTORE start from. */
void map_transition_and_store_darkness_color(s32 frame_duration, const CVECTOR* color) {
    map_transition_darkness_color(frame_duration, color);
    g_map_darkness_color = *color;
}
