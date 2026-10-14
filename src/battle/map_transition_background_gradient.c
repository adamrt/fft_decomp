#include "fft/battle_map.h"
#include "psx/types.h"

/*
 * Starts a fade of both background gradient colors toward `colors` over
 * frame_duration * 8 frames, or applies them at once when frame_duration is
 * zero. Indexing base[i]/color[i] (not advancing pointers) is required: a
 * pointer walk splits into biased givs.
 */
void map_transition_background_gradient(s32 frame_duration, const map_background_gradient_colors_t* colors) {
    map_color_transition_channels_t* base;
    const CVECTOR* color;
    map_background_gradient_colors_t current;
    s32 i;
    s32 red;
    s32 green;
    s32 blue;
    s32 target_red;
    s32 target_green;
    s32 target_blue;

    base = g_map_background_gradient_transition.channels;
    color = colors->colors;
    for (i = 0; i < 2; i++) {
        red = color[i].r << 16;
        target_red = red;
        green = color[i].g << 16;
        target_green = green;
        blue = color[i].b << 16;
        target_blue = blue;
        if (frame_duration != 0) {
            base[i].red_step = (red - (base[i].red & ~0xFFFF)) / (frame_duration * 8);
            base[i].green_step = (green - (base[i].green & ~0xFFFF)) / (frame_duration * 8);
            base[i].blue_step = (blue - (base[i].blue & ~0xFFFF)) / (frame_duration * 8);
        } else {
            base[i].red = target_red;
            base[i].green = target_green;
            base[i].blue = target_blue;
        }
    }
    if (frame_duration != 0) {
        g_map_background_gradient_transition.active = 1;
        g_map_background_gradient_transition.phase = 0;
        g_map_background_gradient_transition.tick = 0;
        g_map_background_gradient_transition.period = frame_duration;
    } else {
        g_map_background_gradient_transition.active = 0;
        current.colors[MAP_BACKGROUND_GRADIENT_TOP].r = base[0].red / 0x10000;
        current.colors[MAP_BACKGROUND_GRADIENT_TOP].g = base[0].green / 0x10000;
        current.colors[MAP_BACKGROUND_GRADIENT_TOP].b = base[0].blue / 0x10000;
        current.colors[MAP_BACKGROUND_GRADIENT_BOTTOM].r = base[1].red / 0x10000;
        current.colors[MAP_BACKGROUND_GRADIENT_BOTTOM].g = base[1].green / 0x10000;
        current.colors[MAP_BACKGROUND_GRADIENT_BOTTOM].b = base[1].blue / 0x10000;
        map_light_state_command(MAP_LIGHT_COMMAND_SET_BACKGROUND_GRADIENT, &current);
    }
}
