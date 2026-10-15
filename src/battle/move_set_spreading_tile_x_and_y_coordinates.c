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

/* Work tile = current tile + one step of the preset direction. */
void move_set_spreading_tile_x_and_y_coordinates(void) {
    move_spread_state_t* spread = g_move_scratch_pad_ptr;
    spread->work_x = spread->tile_x + spread->x_step;
    spread->work_y = spread->tile_y + spread->y_step;
}
