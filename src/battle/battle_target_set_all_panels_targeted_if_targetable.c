#include "fft/battle.h"
#include "psx/types.h"

s32 battle_target_set_all_panels_targeted_if_targetable(void) {
    s32 count;
    s32 i;
    s32 no_tile;
    volatile u8* src;
    targeting_panel_entry_t* dst;

    count = 0;
    i = 0;
    no_tile = MAP_SURFACE_CROSS_SECTION;
    src = (volatile u8*)g_battle_map_tile_data;
    dst = g_battle_target_panel_data;
    do {
        if ((u8)dst->a != 0 && !(src[6] & MAP_TILE_FLAG_BLOCKED) && (src[0] & MAP_SURFACE_MASK) != no_tile) {
            count++;
            src[5] |= MAP_TILE_FLAG_ABILITY_RANGE;
        } else {
            src[5] &= ~MAP_TILE_FLAG_ABILITY_RANGE;
        }
        src += 8;
        i++;
        dst++;
    } while (i < 0x200);
    return count;
}
