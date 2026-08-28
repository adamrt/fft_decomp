#include "fft/battle_move.h"

void battle_move_step_unit_to_map_tile_center_no_height_change(battle_unit_misc_data_t* unit) {
    battle_move_step_unit_to_map_tile_center(unit, 0);
}
