#include "fft/battle_map.h"

void map_set_background_color(s32 red, s32 green, s32 blue) {
    g_map_background_color.r = red;
    g_map_background_color.g = green;
    g_map_background_color.b = blue;
    map_control_gte_background_color(
        BATTLE_BACKGROUND_COLOR_SET, g_map_background_color.r, g_map_background_color.g, g_map_background_color.b);
}
