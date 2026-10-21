/* Save and adjust the battle map ambient light color. */

#include "fft/battle_map_light.h"
#include "fft/gfx.h"
#include "psx/types.h"

static rgb24_t g_battle_map_light_back_color_backup;

static rgb24_t g_current_back_color = { 128, 128, 128 };

/* Save the map's ambient (GTE back) color for battle_map_light_restore_back_color. */
void battle_map_light_save_back_color(void) {
    /* Component copies retain the original byte loads and separate addresses. */
    g_battle_map_light_back_color_backup.r = g_current_back_color.r;
    g_battle_map_light_back_color_backup.g = g_current_back_color.g;
    g_battle_map_light_back_color_backup.b = g_current_back_color.b;
}

/* Reload the saved ambient color and load it into the GTE back color. */
void battle_map_light_restore_back_color(void) {
    g_current_back_color.r = g_battle_map_light_back_color_backup.r;
    g_current_back_color.g = g_battle_map_light_back_color_backup.g;
    g_current_back_color.b = g_battle_map_light_back_color_backup.b;
    battle_map_light_control_back_color(
        BATTLE_MAP_LIGHT_BACK_COLOR_SET, g_current_back_color.r, g_current_back_color.g, g_current_back_color.b);
}

rgb24_t* battle_map_light_get_back_color(void) {
    return &g_current_back_color;
}

/* Set the map's ambient color and load it into the GTE back color; the GTE load is skipped while
 * the back color is disabled. */
void battle_map_light_set_back_color(s32 red, s32 green, s32 blue) {
    g_current_back_color.r = red;
    g_current_back_color.g = green;
    g_current_back_color.b = blue;
    battle_map_light_control_back_color(
        BATTLE_MAP_LIGHT_BACK_COLOR_SET, g_current_back_color.r, g_current_back_color.g, g_current_back_color.b);
}

/* Add per-component deltas to the map ambient color and apply it.
 *
 * A component whose tested delta is negative (`current + delta < current`)
 * becomes 0xff instead. The retail routine tests green with red_delta. */
void battle_map_light_adjust_back_color(s16 red_delta, s16 green_delta, s16 blue_delta) {
    if (g_current_back_color.r + red_delta < g_current_back_color.r) {
        g_current_back_color.r = 0xff;
    } else {
        g_current_back_color.r += red_delta;
    }
    if (g_current_back_color.g + red_delta < g_current_back_color.g) {
        g_current_back_color.g = 0xff;
    } else {
        g_current_back_color.g += green_delta;
    }
    if (g_current_back_color.b + blue_delta < g_current_back_color.b) {
        g_current_back_color.b = 0xff;
    } else {
        g_current_back_color.b += blue_delta;
    }
    battle_map_light_control_back_color(
        BATTLE_MAP_LIGHT_BACK_COLOR_SET, g_current_back_color.r, g_current_back_color.g, g_current_back_color.b);
}
