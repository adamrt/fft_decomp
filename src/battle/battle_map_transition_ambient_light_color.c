#include "fft/map.h"
#include "psx/types.h"

/* Start a fade of the ambient light toward color over frame_duration * 8
 * frames, or apply it at once when frame_duration is zero. */
void battle_map_transition_ambient_light_color(s32 frame_duration, const map_color_t* color) {
    map_color_transition_channels_t* channels;
    map_color_t current;
    s32 red;
    s32 green;
    s32 blue;
    s32 target_red;
    s32 target_green;
    s32 target_blue;

    channels = &g_battle_map_ambient_light_transition.channels;
    /* The separate target copies reproduce the target's register copies that
     * feed the immediate-store path. */
    red = color->red << 16;
    target_red = red;
    green = color->green << 16;
    target_green = green;
    blue = color->blue << 16;
    target_blue = blue;
    if (frame_duration != 0) {
        g_battle_map_ambient_light_transition.channels.red_step
            = (red - (channels->red & ~0xFFFF)) / (frame_duration * 8);
        g_battle_map_ambient_light_transition.channels.green_step
            = (green - (channels->green & ~0xFFFF)) / (frame_duration * 8);
        g_battle_map_ambient_light_transition.channels.blue_step
            = (blue - (channels->blue & ~0xFFFF)) / (frame_duration * 8);
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
    } else {
        g_battle_map_ambient_light_transition.active = 0;
        current.red = channels->red / 0x10000;
        current.green = channels->green / 0x10000;
        current.blue = channels->blue / 0x10000;
        battle_map_light_state_command(MAP_LIGHT_COMMAND_SET_AMBIENT_COLOR, (u8*)&current);
    }
}
