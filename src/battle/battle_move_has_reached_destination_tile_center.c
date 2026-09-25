#include "fft/battle.h"
#include "psx/types.h"

/*
 * Report whether a walking unit has reached the centre of its destination tile
 * along the given facing.  Tile centres are 28 screen units apart with a
 * half-tile offset of 14.
 */
s32 battle_move_has_reached_destination_tile_center(s32 direction, battle_unit_misc_data_t* unit) {
    switch (direction) {
    case 2:
        return unit->screen.vz >= unit->movement.bytes.destination_y * 28 + 14;
    case 0:
        return unit->movement.bytes.destination_y * 28 + 14 >= unit->screen.vz;
    case 3:
        return unit->movement.bytes.destination_x * 28 + 14 >= unit->screen.vx;
    case 1:
        return unit->screen.vx >= unit->movement.bytes.destination_x * 28 + 14;
    }
}
