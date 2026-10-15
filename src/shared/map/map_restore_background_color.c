#include "fft/battle_map.h"

void map_restore_background_color(void) {
    g_map_background_color.r = g_map_background_color_backup.r;
    g_map_background_color.g = g_map_background_color_backup.g;
    g_map_background_color.b = g_map_background_color_backup.b;
    map_control_gte_background_color(
        BATTLE_BACKGROUND_COLOR_SET, g_map_background_color.r, g_map_background_color.g, g_map_background_color.b);
}
