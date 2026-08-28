#include "fft/battle_move.h"

void battle_move_check_and_spread_one_tile_around(void) {
    battle_move_spread_state_t* state;
    s32 i;
    s32 j;

    state = g_battle_move_scratch_pad_ptr;
    battle_move_transfer_tiles_height_halves_and_slope_to_scratch_pad();
    for (i = 0; i < state->outer_count; i++) {
        state->source_unit_record_flag = (i >= 4);
        g_battle_move_spread_preset_table[i % 4]();
        battle_move_set_spreading_tile_x_and_y_coordinates();
        if ((battle_move_check_spreading_tile_coordinates() == 0) && (battle_move_init_source_panel(i) == 0)) {
            for (j = 0; j < state->inner_count; j++) {
                battle_move_propagate_destination(j, 0);
            }
        }
    }
}
