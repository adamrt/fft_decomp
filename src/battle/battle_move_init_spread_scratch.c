#include "fft/battle.h"

void battle_move_init_spread_scratch(void) {
    battle_move_spread_state_t* spread = g_battle_move_scratch_pad_ptr;
    spread->best_height_delta = 0xFF;
    spread->best_fly_height_delta = 0xFF;
    spread->budget_matches = 0;
    spread->candidate_saved = 0;
    spread->best_extra_span = 0x7F;
    battle_move_transfer_tiles_height_halves_and_slope_to_scratch_pad();
}
