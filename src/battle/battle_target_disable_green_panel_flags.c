#include "fft/battle.h"
#include "psx/types.h"

void battle_target_disable_green_panel_flags(void) {
    s32 i;
    volatile u8* tile;
    i = 0;
    tile = (volatile u8*)g_battle_map_tile_data;
    do {
        i++;
        tile[5] &= ~MAP_TILE_FLAG_TARGETED;
        tile += 8;
    } while (i < 0x200);
}
