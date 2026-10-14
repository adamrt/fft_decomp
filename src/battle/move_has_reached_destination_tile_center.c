#include "fft/battle_move.h"
#include "psx/types.h"

/*
 * Report whether a walking unit has reached the center of its destination tile
 * along the given facing.  Tile centers are 28 world units apart with a
 * half-tile offset of 14.
 */
s32 move_has_reached_destination_tile_center(s32 direction, battle_unit_t* unit) {
    switch (direction) {
    case MOVE_DIRECTION_NORTH:
        return unit->world_coords.vz >= unit->movement.destination_y * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
    case MOVE_DIRECTION_SOUTH:
        return unit->movement.destination_y * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2) >= unit->world_coords.vz;
    case MOVE_DIRECTION_WEST:
        return unit->movement.destination_x * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2) >= unit->world_coords.vx;
    case MOVE_DIRECTION_EAST:
        return unit->world_coords.vx >= unit->movement.destination_x * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
    }
}
