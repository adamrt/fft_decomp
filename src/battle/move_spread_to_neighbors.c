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

/* Spread movement to ordinary neighboring candidates.
 *
 * Direction callbacks prepare each candidate; accepted movement updates use
 * zero extra span. Outside AI mode, candidate_saved enables the common finalizer.
 */
void move_spread_to_neighbors(void) {
    move_pathfind_scratch_t* config = g_move_config_ptr;
    move_spread_state_t* state = g_move_scratch_pad_ptr;
    s32 direction;
    s32 candidate;
    s32 callback;

    move_init_spread_scratch();
    for (direction = 0; direction < state->outer_count; direction++) {
        callback = direction % 4;
        g_move_spread_preset_table[callback]();
        move_set_spreading_tile_x_and_y_coordinates();
        if (move_check_spreading_tile_coordinates() == 0 && move_init_source_geometry(direction) == 0) {
            if (state->inner_count > 4 && (config->x != state->work_x || config->y != state->work_y))
                continue;
            for (candidate = 0; candidate < state->inner_count; candidate++) {
                if (move_init_destination_geometry(candidate) == 0 && move_update_candidate(0) == 0
                    && state->budget_matches != 0)
                    move_save_selected_candidate(direction, candidate, 0);
            }
        }
    }
    if (config->ai_propagation_mode == 0 && state->candidate_saved != 0)
        move_apply_selected_candidate(state);
}
