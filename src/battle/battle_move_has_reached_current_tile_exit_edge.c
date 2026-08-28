#include "fft/battle_move.h"
#include "psx/types.h"

/* Mirror of battle_move_has_reached_destination_tile_entry_edge against the tile the unit currently occupies rather
 * than its movement destination. */
s32 battle_move_has_reached_current_tile_exit_edge(s32 direction, battle_unit_misc_data_t* unit) {
    switch (direction) {
    case 2:
        return unit->screen.vz >= (((unit->map_y + 1) * 28) - 7);
    case 0:
        return ((unit->map_y * 28) + 7) >= unit->screen.vz;
    case 3:
        return ((unit->map_x * 28) + 7) >= unit->screen.vx;
    case 1:
        return unit->screen.vx >= (((unit->map_x + 1) * 28) - 7);
    }
}
