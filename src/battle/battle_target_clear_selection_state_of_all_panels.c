#include "fft/battle.h"
#include "psx/types.h"

s32 battle_target_clear_selection_state_of_all_panels(void) {
    s32 count;
    s32 i;
    s32 no_tile;
    volatile map_tile_t* src;
    targeting_panel_entry_t* dst;

    count = 0;
    i = 0;
    no_tile = MAP_SURFACE_CROSS_SECTION;
    src = g_battle_map_tile_data;
    dst = g_battle_target_panel_data;
    do {
        if ((u8)dst->b != 0 && !(src->flags_06.value & MAP_TILE_FLAG_BLOCKED)
            && (src->surface.value & MAP_SURFACE_MASK) != no_tile) {
            count++;
            src->ceiling_depth_and_marks |= MAP_TILE_FLAG_ABILITY_RANGE;
        } else {
            src->ceiling_depth_and_marks &= ~MAP_TILE_FLAG_ABILITY_RANGE;
        }
        src++;
        i++;
        dst++;
    } while (i < 0x200);
    return count;
}
