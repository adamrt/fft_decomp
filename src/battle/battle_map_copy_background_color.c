#include "fft/battle.h"
#include "psx/types.h"

void battle_map_copy_background_color(void) {
    g_battle_map_background_color_backup[0] = g_battle_map_background_color[0];
    g_battle_map_background_color_backup[1] = g_battle_map_background_color[1];
    g_battle_map_background_color_backup[2] = g_battle_map_background_color[2];
}
