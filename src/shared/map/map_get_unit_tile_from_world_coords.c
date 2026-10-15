#include "fft/battle_map.h"

/* The tile under a unit's integer world position (28 units per tile) on its map layer. */
map_tile_t* map_get_unit_tile_from_world_coords(u32 runtime_id) {
    battle_unit_t* unit;

    unit = battle_unit_get_by_runtime_id(runtime_id & 0xffff);
    return map_get_tile_data_pointer(
        unit->world_coords.vx / MAP_TILE_SIZE, unit->world_coords.vz / MAP_TILE_SIZE, unit->map_z);
}
