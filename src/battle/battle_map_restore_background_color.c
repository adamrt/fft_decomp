#include "fft/battle.h"

void battle_map_restore_background_color(void) {
    g_battle_map_background_color[0] = g_battle_map_background_color_backup[0];
    g_battle_map_background_color[1] = g_battle_map_background_color_backup[1];
    g_battle_map_background_color[2] = g_battle_map_background_color_backup[2];
    battle_map_control_gte_background_color(BATTLE_BACKGROUND_COLOR_SET, g_battle_map_background_color[0],
        g_battle_map_background_color[1], g_battle_map_background_color[2]);
}
