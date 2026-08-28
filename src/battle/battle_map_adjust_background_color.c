#include "fft/battle.h"

/* Add per-component deltas to the map background colour and apply it.
 *
 * A component whose tested delta is negative (`current + delta < current`)
 * becomes 0xff instead. The retail routine tests green with red_delta. */
void battle_map_adjust_background_color(s16 red_delta, s16 green_delta, s16 blue_delta) {
    if (g_battle_map_background_color[0] + red_delta < g_battle_map_background_color[0]) {
        g_battle_map_background_color[0] = 0xff;
    } else {
        g_battle_map_background_color[0] += red_delta;
    }
    if (g_battle_map_background_color[1] + red_delta < g_battle_map_background_color[1]) {
        g_battle_map_background_color[1] = 0xff;
    } else {
        g_battle_map_background_color[1] += green_delta;
    }
    if (g_battle_map_background_color[2] + blue_delta < g_battle_map_background_color[2]) {
        g_battle_map_background_color[2] = 0xff;
    } else {
        g_battle_map_background_color[2] += blue_delta;
    }
    battle_map_control_gte_background_color(BATTLE_BACKGROUND_COLOR_SET, g_battle_map_background_color[0],
        g_battle_map_background_color[1], g_battle_map_background_color[2]);
}
