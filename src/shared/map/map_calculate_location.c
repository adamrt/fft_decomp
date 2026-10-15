#include "fft/battle_map.h"
#include "psx/types.h"

/* Index of the unit's tile in g_map_tile_data: layer * 256 + y * map width
 * + x, with the layer in bit 15 of the packed position. */
s32 map_calculate_location(const unit_stats_t* unit) {
    u8 y = unit->position.y;
    u16 elevation = unit->position.higher_elevation;
    u8 x = unit->x;
    return (elevation << 8) + (y * g_map_max_x) + x;
}
