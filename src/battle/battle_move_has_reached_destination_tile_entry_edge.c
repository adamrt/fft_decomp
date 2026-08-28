#include "fft/battle_move.h"
#include "psx/types.h"

/* Whether a walking unit has reached the near edge of its movement destination
 * tile in the given direction.  Tiles are 28 screen units wide and the test
 * keeps a 7-unit margin. */
s32 battle_move_has_reached_destination_tile_entry_edge(s32 direction, battle_unit_misc_data_t* unit) {
    switch (direction) {
    case 2:
        return unit->screen.vz >= ((unit->movement.bytes.destination_y * 28) + 7);
    case 0:
        return (((unit->movement.bytes.destination_y + 1) * 28) - 7) >= unit->screen.vz;
    case 3:
        return (((unit->movement.bytes.destination_x + 1) * 28) - 7) >= unit->screen.vx;
    case 1:
        return unit->screen.vx >= ((unit->movement.bytes.destination_x * 28) + 7);
    }
}
