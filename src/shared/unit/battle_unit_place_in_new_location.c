#include "fft/battle_map.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/*
 * Move a display unit to the supplied tile and facing.
 *
 * Clears the effect vectors, sets both the current and destination tile, and
 * recomputes the integer and fixed-point world positions from the tile.
 */
s32 battle_unit_place_in_new_location(u32 runtime_id, map_tile_coordinates_t* tile, s32 facing) {
    battle_unit_t* unit;

    unit = battle_unit_get_by_runtime_id(runtime_id & 0xFFFF);
    if (unit != 0) {
        unit->event_offset.vz = 0;
        unit->event_offset.vy = 0;
        unit->event_offset.vx = 0;
        unit->seq_offset.vz = 0;
        unit->seq_offset.vy = 0;
        unit->seq_offset.vx = 0;
        unit->screen_offset.vz = 0;
        unit->screen_offset.vy = 0;
        unit->screen_offset.vx = 0;
        unit->map_x = unit->movement.destination_x = tile->x;
        unit->map_y = unit->movement.destination_y = tile->y;
        unit->map_z = unit->movement.destination_z = tile->layer;
        unit->facing = facing << 10;
        unit->world_coords.vx = tile->x * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
        unit->world_coords.vz = tile->y * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
        unit->world_coords.vy = map_calculate_slope_height(&unit->world_coords, unit->map_z);
        unit->real.vx = unit->world_coords.vx * ONE;
        unit->real.vz = unit->world_coords.vz * ONE;
        unit->real.vy = unit->world_coords.vy * ONE;
        if (unit->stats != 0) {
            battle_unit_set_tile_position(unit->stats->battle_id, unit->map_x, unit->map_y, unit->map_z, unit->facing);
        }
        return 1;
    }
    return 0;
}
