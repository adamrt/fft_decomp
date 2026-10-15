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

/*
 * Check a destination path and propagate an improved movement budget.
 *
 * Return 0 for an accepted path, otherwise a numeric rejection reason.
 * The mount helper can update separate state before a later rejection.
 */
s32 move_propagate_destination(s32 candidate, s32 extra_span) {
    move_spread_state_t* state = g_move_scratch_pad_ptr;
    move_pathfind_scratch_t* config = g_move_config_ptr;
    u8* flags = g_move_frontier_flags_ptr;
    move_record_t* records = g_move_records_ptr;
    u8* terrain_costs = g_move_terrain_costs_ptr;
    u8* destination_flags;

    state->work_level = candidate & 1;
    state->destination_unit_record_flag = candidate / 2;
    state->destination_tile = &g_map_tile_data[state->destination_index
        = state->work_level * MAP_TILES_PER_LAYER + state->work_y * config->map_max_x + state->work_x];
    state->destination_panel = &g_target_panels[state->destination_index];
    state->destination_base_height_times_two = state->destination_tile->height * 2;
    state->destination_half_height = state->destination_tile->depth_half_height.half_height;
    state->destination_slope = state->destination_tile->slope_type;
    destination_flags = &flags[state->destination_index];
    if (state->destination_unit_record_flag != 0) {
        if (!(*destination_flags & (FRONTIER_FLAG_MOUNTABLE_UNIT | FRONTIER_FLAG_STAND_ON_UNIT)))
            return 1; /* no unit there to mount or stand on */
        state->destination_unit_record_index = state->destination_panel->unit_record_index;
        state->destination_entry_height = records[state->destination_unit_record_index].top_height;
        state->destination_ceiling_height = move_calculate_tile_ceiling(
            state->work_x, state->work_y, state->destination_side_shift, (state->destination_entry_height - 1));
        if (*destination_flags & FRONTIER_FLAG_MOUNTABLE_UNIT)
            move_check_occupied_tile_standing_height(state->destination_unit_record_index, extra_span);
        if (!(*destination_flags & FRONTIER_FLAG_STAND_ON_UNIT))
            return 17; /* the unit cannot be stood on (a mount was handled by the helper) */
    } else {
        if (!(*destination_flags & FRONTIER_FLAG_ENTERABLE))
            return 2; /* the tile is not enterable */
        state->destination_entry_height = state->destination_base_height_times_two
            + state->destination_half_height * ((state->destination_slope >> state->destination_side_shift) & 3);
        state->destination_opposite_height = state->destination_base_height_times_two
            + state->destination_half_height * ((state->destination_slope >> state->source_side_shift) & 3);
        state->destination_side_height_delta = state->destination_opposite_height - state->destination_entry_height;
        state->destination_ceiling_height = move_calculate_tile_ceiling(
            state->work_x, state->work_y, state->destination_side_shift, state->destination_entry_height);
        if (state->destination_side_height_delta > 0 && (*destination_flags & FRONTIER_FLAG_STEEP))
            return 3; /* a steep slope entered from its low side */
    }
    if (state->destination_ceiling_height < state->source_exit_height + config->unit_size)
        return 4; /* too low a ceiling over the destination for the mover's exit height */
    if (state->source_ceiling_height < state->destination_entry_height + config->unit_size)
        return 5; /* too low a ceiling over the source for the destination's entry height */
    if (state->destination_ceiling_height < state->destination_entry_height + config->unit_size)
        return 6; /* no headroom on the destination itself */
    if (state->destination_unit_record_flag == 0)
        state->destination_entry_height += state->destination_tile->depth_half_height.depth * 2;
    if (state->destination_entry_height > state->source_exit_height) {
        if (extra_span != 0)
            return 8; /* a step that spans tiles cannot climb */
        state->height_delta = state->destination_entry_height - state->source_exit_height;
    } else {
        state->height_delta = state->source_exit_height - state->destination_entry_height;
    }
    if (state->height_delta > config->jump_times_two)
        return 9; /* beyond the jump limit */
    state->work_height = state->source_panel->max_height_delta;
    state->height_delta = state->work_height < state->height_delta ? state->height_delta : state->work_height;
    if (config->move_type == MOVE_CLASS_FLY)
        state->height_delta = 0;
    if (state->destination_unit_record_flag != 0) {
        state->candidate_remaining_range = state->source_panel->remaining_range - 1 - extra_span;
        if (state->candidate_remaining_range <= 0)
            return 11; /* no range left to step onto the unit */
        if ((u8)state->candidate_remaining_range
            < g_target_panels[state->destination_unit_record_index + MAP_TILE_COUNT].remaining_range)
            return 12; /* the unit top is already reached with more range left */
        if ((u8)state->candidate_remaining_range
                == g_target_panels[state->destination_unit_record_index + MAP_TILE_COUNT].remaining_range
            && state->height_delta
                >= g_target_panels[state->destination_unit_record_index + MAP_TILE_COUNT].max_height_delta)
            return 13; /* same range left, and no smaller height difference */
        g_target_panels[state->destination_unit_record_index + MAP_TILE_COUNT].remaining_range
            = (u8)state->candidate_remaining_range;
        g_target_panels[state->destination_unit_record_index + MAP_TILE_COUNT].max_height_delta = state->height_delta;
    } else {
        state->candidate_remaining_range
            = state->source_panel->remaining_range - extra_span - terrain_costs[state->destination_tile->surface.type];
        if (state->candidate_remaining_range <= 0)
            return 14; /* no range left after the terrain cost */
        if ((u8)state->candidate_remaining_range < state->destination_panel->remaining_range)
            return 15; /* the tile is already reached with more range left */
        if ((u8)state->candidate_remaining_range == state->destination_panel->remaining_range
            && state->height_delta >= state->destination_panel->max_height_delta)
            return 16; /* same range left, and no smaller height difference */
        state->destination_panel->remaining_range = (u8)state->candidate_remaining_range;
        state->destination_panel->max_height_delta = state->height_delta;
    }
    if (state->candidate_remaining_range >= 2) {
        g_move_frontier_flags_ptr[state->destination_index] |= FRONTIER_FLAG_QUEUED;
        if ((u8)state->candidate_remaining_range > state->frontier_max_remaining_range)
            state->frontier_max_remaining_range = (u8)state->candidate_remaining_range;
    }
    return 0;
}
