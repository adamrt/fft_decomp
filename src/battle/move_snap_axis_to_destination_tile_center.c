#include "fft/battle_move.h"
#include "psx/types.h"

enum {
    TILE_CENTER_OFFSET = MAP_TILE_SIZE / 2,
    REAL_COORD_SHIFT = 12,
};

/*
 * Snaps the unit's screen/real coordinate on the axis of travel to the center
 * of its movement destination tile: south/north (0, 2) set Y, west/east (1, 3)
 * set X.
 */
void move_snap_axis_to_destination_tile_center(s32 direction, battle_unit_t* unit) {
    s16 coordinate;

    switch (direction) {
    case MOVE_DIRECTION_NORTH:
    case MOVE_DIRECTION_SOUTH:
        coordinate = unit->movement.destination_y * MAP_TILE_SIZE + TILE_CENTER_OFFSET;
        unit->world_coords.vz = coordinate;
        unit->real.vz = coordinate << REAL_COORD_SHIFT;
        return;
    case MOVE_DIRECTION_EAST:
    case MOVE_DIRECTION_WEST:
        coordinate = unit->movement.destination_x * MAP_TILE_SIZE + TILE_CENTER_OFFSET;
        unit->world_coords.vx = coordinate;
        unit->real.vx = coordinate << REAL_COORD_SHIFT;
        return;
    }
}
