#include "fft/battle_move.h"

/* Distortion animation 0x09: move to the center of the unit's map tile without
 * changing height. */
void move_step_unit_to_map_tile_center_no_height_change(battle_unit_t* unit) {
    move_step_unit_to_map_tile_center(unit, 0);
}
