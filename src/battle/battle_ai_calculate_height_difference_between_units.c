#include "fft/battle_ai.h"
#include "fft/map.h"

/*
 * Compare the unit tile's height with the supplied AI coordinates.
 *
 * Return their signed half-height difference divided by 2, truncating
 * toward 0. unit_t body height and water depth are not included.
 */
s32 battle_ai_calculate_height_difference_between_units(battle_ai_coords_t* coords, battle_stats_t* unit) {
    map_tile_t* tile;
    s32 h1;
    s32 h2;

    tile = &g_battle_map_tile_data[(unit->position.raw >> 15) * 0x100
        + unit->position.bits.y * g_battle_ai_data_base.map_max_x + unit->x];
    h1 = tile->height * 2 + (tile->depth_half_height & MAP_TILE_HALF_HEIGHT_MASK);
    tile = &g_battle_map_tile_data[(coords->bytes.elevation << 8) + coords->bytes.y * g_battle_ai_data_base.map_max_x
        + coords->bytes.x];
    h2 = tile->height * 2 + (tile->depth_half_height & MAP_TILE_HALF_HEIGHT_MASK);
    return (h1 - h2) / 2;
}
