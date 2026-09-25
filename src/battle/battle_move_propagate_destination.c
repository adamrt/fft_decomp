#include "fft/battle.h"

/*
 * Check a destination path and propagate an improved movement budget.
 *
 * Return 0 for an accepted path, otherwise a numeric rejection reason.
 * The mount helper can update separate state before a later rejection.
 */
s32 battle_move_propagate_destination(s32 candidate, s32 extra_span) {
    battle_move_spread_state_t* state = g_battle_move_scratch_pad_ptr;
    battle_move_pathfind_scratch_t* config = g_battle_move_config_ptr;
    u8* flags = g_battle_move_frontier_flags_ptr;
    battle_move_record_t* records = g_battle_move_records_ptr;
    u8* terrain_costs = g_battle_move_terrain_costs_ptr;
    u8* destination_flags;

    state->work_level = candidate & 1;
    state->destination_unit_record_flag = candidate / 2;
    state->destination_tile = &g_battle_map_tile_data[state->destination_index
        = state->work_level * 256 + state->work_y * config->map_max_x + (u16)state->work_x];
    state->destination_panel = &g_battle_target_panels[state->destination_index];
    state->destination_base_height_times_two = state->destination_tile->height * 2;
    state->destination_half_height = state->destination_tile->depth_half_height & MAP_TILE_HALF_HEIGHT_MASK;
    state->destination_slope = state->destination_tile->slope_type;
    destination_flags = &flags[state->destination_index];
    if (state->destination_unit_record_flag != 0) {
        if (!(*destination_flags & 0x28))
            return 1;
        state->destination_unit_record_index = state->destination_panel->unit_record_index;
        state->destination_entry_height = records[state->destination_unit_record_index].top_height;
        state->destination_ceiling_height = battle_move_calculate_tile_ceiling(
            state->work_x, state->work_y, state->destination_side_shift, (u8)(state->destination_entry_height - 1));
        if (*destination_flags & FRONTIER_FLAG_VALID_DESTINATION)
            battle_move_check_occupied_tile_standing_height(state->destination_unit_record_index, extra_span);
        if (!(*destination_flags & 8))
            return 17;
    } else {
        if (!(*destination_flags & 0x10))
            return 2;
        state->destination_entry_height = state->destination_base_height_times_two
            + state->destination_half_height * ((state->destination_slope >> state->destination_side_shift) & 3);
        state->destination_opposite_height = state->destination_base_height_times_two
            + state->destination_half_height * ((state->destination_slope >> state->source_side_shift) & 3);
        state->destination_side_height_delta = state->destination_opposite_height - state->destination_entry_height;
        state->destination_ceiling_height = battle_move_calculate_tile_ceiling(
            state->work_x, state->work_y, state->destination_side_shift, state->destination_entry_height);
        if (state->destination_side_height_delta > 0 && (*destination_flags & 0x40))
            return 3;
    }
    if (state->destination_ceiling_height < state->source_exit_height + config->unit_size)
        return 4;
    if (state->source_ceiling_height < state->destination_entry_height + config->unit_size)
        return 5;
    if (state->destination_ceiling_height < state->destination_entry_height + config->unit_size)
        return 6;
    if (state->destination_unit_record_flag == 0)
        state->destination_entry_height += (state->destination_tile->depth_half_height >> MAP_TILE_DEPTH_SHIFT) * 2;
    if (state->destination_entry_height > state->source_exit_height) {
        if (extra_span != 0)
            return 8;
        state->height_delta = state->destination_entry_height - state->source_exit_height;
    } else {
        state->height_delta = state->source_exit_height - state->destination_entry_height;
    }
    if (state->height_delta > config->jump_times_two)
        return 9;
    state->work_height = state->source_panel->max_height_delta;
    state->height_delta = state->work_height < state->height_delta ? state->height_delta : state->work_height;
    if (config->move_type == BATTLE_MOVEMENT_CLASS_FLY)
        state->height_delta = 0;
    if (state->destination_unit_record_flag != 0) {
        state->candidate_remaining_range.value = state->source_panel->remaining_range - 1 - extra_span;
        if (state->candidate_remaining_range.value <= 0)
            return 11;
        if (state->candidate_remaining_range.bytes.low
            < g_battle_target_panels[state->destination_unit_record_index + 512].remaining_range)
            return 12;
        if (state->candidate_remaining_range.bytes.low
                == g_battle_target_panels[state->destination_unit_record_index + 512].remaining_range
            && state->height_delta
                >= g_battle_target_panels[state->destination_unit_record_index + 512].max_height_delta)
            return 13;
        g_battle_target_panels[state->destination_unit_record_index + 512].remaining_range
            = state->candidate_remaining_range.bytes.low;
        g_battle_target_panels[state->destination_unit_record_index + 512].max_height_delta = state->height_delta;
    } else {
        state->candidate_remaining_range.value = state->source_panel->remaining_range - extra_span
            - terrain_costs[state->destination_tile->surface.value & MAP_SURFACE_MASK];
        if (state->candidate_remaining_range.value <= 0)
            return 14;
        if (state->candidate_remaining_range.bytes.low < state->destination_panel->remaining_range)
            return 15;
        if (state->candidate_remaining_range.bytes.low == state->destination_panel->remaining_range
            && state->height_delta >= state->destination_panel->max_height_delta)
            return 16;
        state->destination_panel->remaining_range = state->candidate_remaining_range.bytes.low;
        state->destination_panel->max_height_delta = state->height_delta;
    }
    if (state->candidate_remaining_range.value >= 2) {
        g_battle_move_frontier_flags_ptr[state->destination_index] |= 1;
        if (state->candidate_remaining_range.bytes.low > state->frontier_max_remaining_range)
            state->frontier_max_remaining_range = state->candidate_remaining_range.bytes.low;
    }
    return 0;
}
