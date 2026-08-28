#include "fft/battle.h"
#include "fft/map.h"
#include "psx/types.h"

s32 battle_target_set_all_panels_targeted_if_targetable(void) {
    s32 count;
    s32 i;
    s32 no_tile;
    volatile u8* src;
    u8* dst;

    count = 0;
    i = 0;
    no_tile = MAP_SURFACE_CROSS_SECTION;
    src = (volatile u8*)g_battle_map_tile_data;
    dst = (u8*)g_battle_target_panel_data;
    do {
        if (dst[0] != 0 && !(src[6] & MAP_TILE_FLAG_BLOCKED) && (src[0] & MAP_SURFACE_MASK) != no_tile) {
            count++;
            src[5] |= MAP_TILE_FLAG_ABILITY_RANGE;
        } else {
            src[5] &= ~MAP_TILE_FLAG_ABILITY_RANGE;
        }
        src += 8;
        i++;
        dst += 5;
    } while (i < 0x200);
    return count;
}
