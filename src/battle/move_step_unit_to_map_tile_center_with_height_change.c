#include "fft/battle_move.h"

/* Distortion animation 0x05: move to the center of the unit's map tile in equal
 * steps, height included (move_step_unit_to_map_tile_center). */
void move_step_unit_to_map_tile_center_with_height_change(battle_unit_t* unit) {
    move_step_unit_to_map_tile_center(unit, 1);
}
