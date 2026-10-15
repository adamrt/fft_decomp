#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_ai.h"
#include "fft/battle_map.h"
#include "fft/battle_move.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "fft/memory.h"
#include "psx/libetc.h"
#include "psx/types.h"

/*
 * Check whether the spreading coordinates lie outside the map.
 *
 * Return 1 for an invalid coordinate, or 0 for an in-bounds tile.
 */
s32 move_check_spreading_tile_coordinates(void) {
    move_spread_state_t* state = g_move_scratch_pad_ptr;
    move_pathfind_scratch_t* config = g_move_config_ptr;
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
