#include "fft/battle_move.h"
#include "psx/types.h"

void battle_move_set_spreading_tile_x_and_y_coordinates(void) {
    battle_move_spread_state_t* spread = g_battle_move_scratch_pad_ptr;
    spread->work_x = spread->tile_x + spread->x_step;
    spread->work_y = spread->tile_y + spread->y_step;
}
