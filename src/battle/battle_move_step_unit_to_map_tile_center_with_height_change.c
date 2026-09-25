#include "fft/battle.h"

/* The 0x80089ba0 callee uses fixed per-frame increments, unlike the
 * acceleration routine at 0x800898a0. */
void battle_move_step_unit_to_map_tile_center_with_height_change(battle_unit_misc_data_t* unit) {
    battle_move_step_unit_to_map_tile_center(unit, 1);
}
