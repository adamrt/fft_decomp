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

/* Check a movement candidate and update its mode-dependent budget state.
 *
 * AI mode returns 11 after an accepted update; panel byte 4 breaks equal
 * budgets by the smaller immediate height difference. Non-AI mode returns 0
 * after geometry checks and sets budget_matches only when the budget matches.
 */
s32 move_update_candidate(s32 extra_span) {
    move_spread_state_t* state = g_move_scratch_pad_ptr;
    move_pathfind_scratch_t* config = g_move_config_ptr;
    u8* terrain_costs = g_move_terrain_costs_ptr;
    s32 step_cost;

    if (state->source_ceiling_height < state->destination_entry_height + config->unit_size)
        return 1;
    if (state->destination_ceiling_height < state->clearance_height + config->unit_size)
        return 2;
    if (state->source_ceiling_height < state->clearance_height + config->unit_size)
        return 3;
    if (state->destination_entry_height < state->source_exit_height) {
        if (extra_span != 0)
            return 4;
        state->height_delta = state->source_exit_height - state->destination_entry_height;
    } else {
        state->height_delta = state->destination_entry_height - state->source_exit_height;
    }
    if (state->height_delta > config->jump_times_two)
        return 7;

    if (config->ai_propagation_mode != 0) {
        state->candidate_remaining_range
            = state->current_panel->remaining_range - extra_span - terrain_costs[state->current_tile->surface.type];
        if (state->candidate_remaining_range <= 0)
            return 8;
        if ((u8)state->candidate_remaining_range < state->destination_panel->remaining_range)
            return 9;
        if ((u8)state->candidate_remaining_range == state->destination_panel->remaining_range
            && state->height_delta >= state->destination_panel->max_height_delta)
            return 10;
        state->destination_panel->remaining_range = (u8)state->candidate_remaining_range;
        state->destination_panel->max_height_delta = state->height_delta;
        if (state->candidate_remaining_range >= 2) {
            g_move_frontier_flags_ptr[state->destination_index] |= FRONTIER_FLAG_QUEUED;
            if ((u8)state->candidate_remaining_range > state->frontier_max_remaining_range)
                state->frontier_max_remaining_range = (u8)state->candidate_remaining_range;
        }
        return 11;
    }
    step_cost = extra_span + 1;
    if (state->outer_count >= 5 || state->source_unit_record_flag != 0) {
        if (state->candidate_remaining_range == state->effective_destination_panel->remaining_range - step_cost)
            state->budget_matches = 1;
    } else {
        if (state->candidate_remaining_range
            == state->effective_destination_panel->remaining_range
                - g_move_terrain_costs_ptr[state->current_tile->surface.type] - extra_span)
            state->budget_matches = 1;
    }
    return 0;
}
