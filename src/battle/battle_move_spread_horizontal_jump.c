#include "fft/battle.h"

/* Spread movement across horizontal jumps of up to half the acting unit's Jump.
 *
 * The first four directions spread from the tile and the rest from above the
 * unit (source_unit_record_flag). Each pass continues only while the source panel's remaining
 * range minus the jump span stays at least 2 and both spans are clear. */
void battle_move_spread_horizontal_jump(void) {
    battle_move_pathfind_scratch_t* config = g_battle_move_config_ptr;
    battle_move_spread_state_t* state = g_battle_move_scratch_pad_ptr;
    s32 outer_index;
    s32 inner_index;
    s32 pass_index;

    battle_move_transfer_tiles_height_halves_and_slope_to_scratch_pad();
    for (outer_index = 0; outer_index < state->outer_count; outer_index++) {
        state->source_unit_record_flag = outer_index >= 4;
        g_battle_move_spread_preset_table[outer_index % 4]();
        if (battle_move_init_source_panel(outer_index) == 0) {
            battle_move_set_spreading_tile_x_and_y_coordinates();
            for (pass_index = 1; pass_index <= config->jump_half; pass_index++) {
                state->jump_remaining_range = state->source_panel->remaining_range - pass_index;
                if (state->jump_remaining_range < 2) {
                    break;
                }
                if (battle_move_check_spreading_tile_coordinates() != 0) {
                    break;
                }
                if (battle_move_check_horizontal_jump(state->work_y * config->map_max_x + state->work_x,
                        state->source_side_shift, state->destination_side_shift, state->source_exit_height)
                    != 0) {
                    break;
                }
                state->work_x = state->tile_x + state->x_step * (pass_index + 1);
                state->work_y = state->tile_y + state->y_step * (pass_index + 1);
                if (battle_move_check_spreading_tile_coordinates() != 0) {
                    break;
                }
                for (inner_index = 0; inner_index < state->inner_count; inner_index++) {
                    battle_move_propagate_destination(inner_index, pass_index);
                }
            }
        }
    }
}
