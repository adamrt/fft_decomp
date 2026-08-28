#include "fft/battle_move.h"

/* Spread movement to ordinary neighboring candidates.
 *
 * Direction callbacks prepare each candidate; accepted movement updates use
 * zero extra span. Outside AI mode, candidate_saved enables the common finalizer.
 */
void battle_move_spread_to_neighbors(void) {
    battle_move_pathfind_scratch_t* config = g_battle_move_config_ptr;
    battle_move_spread_state_t* state = g_battle_move_scratch_pad_ptr;
    s32 direction;
    s32 candidate;
    s32 callback;

    battle_move_init_spread_scratch();
    for (direction = 0; direction < state->outer_count; direction++) {
        callback = direction % 4;
        g_battle_move_spread_preset_table[callback]();
        battle_move_set_spreading_tile_x_and_y_coordinates();
        if (battle_move_check_spreading_tile_coordinates() == 0 && battle_move_init_source_geometry(direction) == 0) {
            if (state->inner_count > 4 && (config->x != state->work_x || config->y != state->work_y))
                continue;
            for (candidate = 0; candidate < state->inner_count; candidate++) {
                if (battle_move_init_destination_geometry(candidate) == 0 && battle_move_update_candidate(0) == 0
                    && state->budget_matches != 0)
                    battle_move_save_selected_candidate(direction, candidate, 0);
            }
        }
    }
    if (config->ai_propagation_mode == 0 && state->candidate_saved != 0)
        battle_move_apply_selected_candidate(state);
}
