#include "fft/battle_move.h"
#include "psx/types.h"

void battle_move_spread_to_north_preset(void) {
    battle_move_spread_state_t* spread = g_battle_move_scratch_pad_ptr;
    spread->y_step = 1;
    spread->source_side_shift = 6;
    spread->x_step = 0;
    spread->destination_side_shift = 4;
}
