#include "fft/battle_move.h"

/*
 * Check whether the spreading coordinates lie outside the map.
 *
 * Return 1 for an invalid coordinate, or 0 for an in-bounds tile.
 */
s32 battle_move_check_spreading_tile_coordinates(void) {
    battle_move_spread_state_t* state = g_battle_move_scratch_pad_ptr;
    battle_move_pathfind_scratch_t* config = g_battle_move_config_ptr;
    s32 x = state->work_x;
    s32 y;
    if (x >= 0) {
        y = state->work_y;
        if (y < 0)
            return 1;
        if (x >= config->map_max_x)
            return 1;
        if (y < config->map_max_y)
            return 0;
    }
    return 1;
}
