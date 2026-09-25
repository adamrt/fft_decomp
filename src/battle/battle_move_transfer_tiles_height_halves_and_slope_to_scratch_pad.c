#include "fft/battle.h"

void battle_move_transfer_tiles_height_halves_and_slope_to_scratch_pad(void) {
    battle_move_spread_state_t* state = g_battle_move_scratch_pad_ptr;
    state->source_base_height_times_two = state->current_tile->height << 1;
    state->source_half_height = state->current_tile->depth_half_height & MAP_TILE_HALF_HEIGHT_MASK;
    state->source_slope = state->current_tile->slope_type;
}
