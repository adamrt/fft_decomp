#include "fft/battle.h"

void battle_move_calculate_spread(void) {
    battle_move_pathfind_scratch_t* config = g_battle_move_config_ptr;
    battle_move_spread_state_t* state = g_battle_move_scratch_pad_ptr;
    s32 outer_index;
    s32 inner_index;
    s32 pass_index;
    s32 path_index;
    s32 callback_index;

    battle_move_init_spread_scratch();
    for (outer_index = 0; outer_index < state->outer_count; outer_index++) {
        callback_index = outer_index % 4;
        g_battle_move_spread_preset_table[callback_index]();
        if (battle_move_init_source_geometry(outer_index) == 0) {
            pass_index = 1;
            battle_move_set_spreading_tile_x_and_y_coordinates();
            if (config->jump_half != 0) {
                do {
                    if (battle_move_check_spreading_tile_coordinates() == 0) {
                        state->work_x = state->tile_x + state->x_step * (pass_index + 1);
                        state->work_y = state->tile_y + state->y_step * (pass_index + 1);
                        if (battle_move_check_spreading_tile_coordinates() == 0) {
                            for (inner_index = 0; inner_index < state->inner_count; inner_index++) {
                                if (battle_move_init_destination_geometry(inner_index) == 0) {
                                    state->placement_failed = 0;
                                    for (path_index = 1; path_index <= pass_index; path_index++) {
                                        if (battle_move_check_horizontal_jump(
                                                (state->work_y - state->y_step * path_index) * config->map_max_x
                                                    + (state->work_x - state->x_step * path_index),
                                                state->source_side_shift, state->destination_side_shift,
                                                state->destination_entry_height)
                                            != 0) {
                                            state->placement_failed = 1;
                                            break;
                                        }
                                    }
                                    if (state->placement_failed == 0 && battle_move_update_candidate(pass_index) == 0
                                        && state->budget_matches != 0) {
                                        battle_move_save_selected_candidate(outer_index, inner_index, pass_index);
                                    }
                                }
                            }
                        } else {
                            break;
                        }
                    } else {
                        break;
                    }
                    pass_index++;
                } while (pass_index <= config->jump_half);
            }
        }
    }
    if (config->ai_propagation_mode == 0 && state->candidate_saved != 0) {
        battle_move_apply_selected_candidate(state);
    }
}
