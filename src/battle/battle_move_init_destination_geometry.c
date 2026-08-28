#include "fft/battle_ai.h"
#include "fft/battle_move.h"
#include "fft/map.h"

/* Prepare destination geometry for a movement candidate.
 *
 * Tile candidates use the selected map layer. unit_t-record candidates use
 * compact standing/top heights and an auxiliary targeting panel.
 */
s32 battle_move_init_destination_geometry(s32 candidate) {
    battle_move_pathfind_scratch_t* config = g_battle_move_config_ptr;
    battle_move_spread_state_t* state = g_battle_move_scratch_pad_ptr;
    u8* tile_flags = g_battle_move_frontier_flags_ptr;
    battle_move_record_t* records = g_battle_move_records_ptr;

    state->destination_unit_record_flag = candidate >= 2;
    state->work_level = candidate & 1;
    if (state->inner_count >= 5)
        state->work_level = config->high_elevation;
    state->destination_index = state->work_level * 256 + state->work_y * config->map_max_x + (u16)state->work_x;
    state->destination_tile = &g_battle_map_tile_data[state->destination_index];
    state->destination_panel = &g_battle_target_panels[state->destination_index];
    state->destination_base_height_times_two = state->destination_tile->height * 2;
    state->destination_half_height = state->destination_tile->depth_half_height & MAP_TILE_HALF_HEIGHT_MASK;
    state->destination_slope = state->destination_tile->slope_type;
    if (state->inner_count >= 5 || state->destination_unit_record_flag != 0) {
        state->destination_unit_record_index = state->destination_panel->unit_record_index;
        if (state->inner_count >= 5) {
            state->destination_unit_record_flag = 1;
            if (!(tile_flags[state->destination_index] & FRONTIER_FLAG_VALID_DESTINATION))
                return 1;
            state->destination_entry_height = records[state->destination_unit_record_index].standing_height
                + g_battle_move_height_offsets[candidate];
            state->work_height = state->destination_base_height_times_two
                + state->destination_half_height * ((state->destination_slope >> state->destination_side_shift) & 3);
            if (state->destination_entry_height < state->work_height)
                return 2;
        } else {
            if (!(tile_flags[state->destination_index] & 8))
                return 3;
            state->destination_entry_height = records[state->destination_unit_record_index].top_height;
        }
        state->destination_side_height_delta = 0;
        state->destination_opposite_height = state->destination_entry_height;
        state->destination_ceiling_height = battle_move_calculate_tile_ceiling(
            state->work_x, state->work_y, state->destination_side_shift, (u8)(state->destination_entry_height - 1));
        state->destination_panel_index = state->destination_unit_record_index + 512;
        state->effective_destination_panel = &g_battle_target_panels[state->destination_panel_index];
    } else {
        if (!(tile_flags[state->destination_index] & 0x10))
            return 4;
        state->destination_entry_height = state->destination_base_height_times_two
            + state->destination_half_height * ((state->destination_slope >> state->destination_side_shift) & 3);
        state->destination_opposite_height = state->destination_base_height_times_two
            + state->destination_half_height * ((state->destination_slope >> state->source_side_shift) & 3);
        state->destination_side_height_delta = state->destination_entry_height - state->destination_opposite_height;
        state->destination_ceiling_height = battle_move_calculate_tile_ceiling(
            state->work_x, state->work_y, state->destination_side_shift, state->destination_entry_height);
        if (state->destination_side_height_delta >= 0 && (tile_flags[state->destination_index] & 0x40))
            return 5;
        state->clearance_height = state->source_exit_height;
        if ((state->destination_tile->depth_half_height & MAP_TILE_DEPTH_MASK) && (config->movement_set_3 & 0xc0)) {
            state->destination_entry_height += (state->destination_tile->depth_half_height >> MAP_TILE_DEPTH_SHIFT) * 2;
            if (config->movement_set_3 & 0x40)
                state->destination_entry_height -= 2;
        }
        state->destination_panel_index = state->destination_index;
        state->effective_destination_panel = &g_battle_target_panels[state->destination_panel_index];
    }
    return 0;
}
