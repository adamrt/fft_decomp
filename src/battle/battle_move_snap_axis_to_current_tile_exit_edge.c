#include "fft/battle_move.h"
#include "psx/types.h"

enum { TILE_SIZE_UNITS = 0x1c, TILE_EDGE_MARGIN = 7, REAL_COORD_SHIFT = 12 };

/*
 * Snaps the unit's screen/real coordinate on the axis of travel to the margin
 * band of the tile it currently occupies, mirroring
 * battle_move_snap_axis_to_destination_tile_center against map_x/map_y.
 */
void battle_move_snap_axis_to_current_tile_exit_edge(s32 direction, battle_unit_misc_data_t* unit) {
    s16 coordinate;

    switch (direction) {
    case 2:
        coordinate = (unit->map_y + 1) * TILE_SIZE_UNITS - TILE_EDGE_MARGIN;
        unit->screen.vz = coordinate;
        unit->real.vz = coordinate << REAL_COORD_SHIFT;
        return;
    case 0:
        coordinate = unit->map_y * TILE_SIZE_UNITS + TILE_EDGE_MARGIN;
        unit->screen.vz = coordinate;
        unit->real.vz = coordinate << REAL_COORD_SHIFT;
        return;
    case 3:
        coordinate = unit->map_x * TILE_SIZE_UNITS + TILE_EDGE_MARGIN;
        unit->screen.vx = coordinate;
        unit->real.vx = coordinate << REAL_COORD_SHIFT;
        return;
    case 1:
        coordinate = (unit->map_x + 1) * TILE_SIZE_UNITS - TILE_EDGE_MARGIN;
        unit->screen.vx = coordinate;
        unit->real.vx = coordinate << REAL_COORD_SHIFT;
        return;
    }
}
