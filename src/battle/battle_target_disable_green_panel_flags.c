#include "fft/battle.h"
#include "psx/types.h"

void battle_target_disable_green_panel_flags(void) {
    s32 i;
    volatile map_tile_t* tile;
    i = 0;
    tile = g_battle_map_tile_data;
    do {
        i++;
        tile->ceiling_depth_and_marks &= ~MAP_TILE_FLAG_TARGETED;
        tile++;
    } while (i < 0x200);
}
