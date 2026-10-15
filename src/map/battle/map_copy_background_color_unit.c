#include "fft/battle_map.h"
#include "psx/types.h"

void map_copy_background_color(void) {
    /* Component copies retain the original byte loads and separate addresses. */
    g_map_background_color_backup.r = g_map_background_color.r;
    g_map_background_color_backup.g = g_map_background_color.g;
    g_map_background_color_backup.b = g_map_background_color.b;
}

void map_restore_background_color(void) {
    g_map_background_color.r = g_map_background_color_backup.r;
    g_map_background_color.g = g_map_background_color_backup.g;
    g_map_background_color.b = g_map_background_color_backup.b;
    map_control_gte_background_color(
        BATTLE_BACKGROUND_COLOR_SET, g_map_background_color.r, g_map_background_color.g, g_map_background_color.b);
}

rgb24_t* map_get_background_color(void) {
    return &g_map_background_color;
}

void map_set_background_color(s32 red, s32 green, s32 blue) {
    g_map_background_color.r = red;
    g_map_background_color.g = green;
    g_map_background_color.b = blue;
    map_control_gte_background_color(
        BATTLE_BACKGROUND_COLOR_SET, g_map_background_color.r, g_map_background_color.g, g_map_background_color.b);
}

/* Add per-component deltas to the map background color and apply it.
 *
 * A component whose tested delta is negative (`current + delta < current`)
 * becomes 0xff instead. The retail routine tests green with red_delta. */
void map_adjust_background_color(s16 red_delta, s16 green_delta, s16 blue_delta) {
    if (g_map_background_color.r + red_delta < g_map_background_color.r) {
        g_map_background_color.r = 0xff;
    } else {
        g_map_background_color.r += red_delta;
    }
    if (g_map_background_color.g + red_delta < g_map_background_color.g) {
        g_map_background_color.g = 0xff;
    } else {
        g_map_background_color.g += green_delta;
    }
    if (g_map_background_color.b + blue_delta < g_map_background_color.b) {
        g_map_background_color.b = 0xff;
    } else {
        g_map_background_color.b += blue_delta;
    }
    map_control_gte_background_color(
        BATTLE_BACKGROUND_COLOR_SET, g_map_background_color.r, g_map_background_color.g, g_map_background_color.b);
}
