#include "fft/battle.h"

/*
 * Reject a facing candidate outside the map or too far above the source.
 *
 * Return 1 for rejection and 0 otherwise. Tile flag 0 also rejects a candidate.
 * Only the destination's lower layer is checked; a rise of 3 is rejected,
 * but descents are unrestricted. Slope and depth do not affect this test.
 */
s32 battle_ai_check_facing_tile(battle_ai_coords_t* source, battle_ai_coords_t* target) {
    s32 x = target->bytes.x;
    s32 width = g_battle_ai_data_base.map_max_x;
    s32 y;
    map_tile_t* tile;

    if ((u32)x >= (u32)width) {
        return 1;
    }
    y = target->bytes.y;
    if ((u32)y >= g_battle_ai_data_base.map_max_y
        || (tile = &g_battle_map_tile_data[y * width + x])->flags_06.bits.blocked) {
        return 1;
    }
    return tile->height
        - g_battle_map_tile_data[(source->bytes.elevation << 8) + source->bytes.y * width + source->bytes.x].height
        >= 3;
}
