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
 * Select a source movement panel and prepare its exit height.
 *
 * Return 1 for a low extended sample, 2 for an unavailable unit-top path,
 * 3 for a restricted terrain exit, or 4 for a panel with less than 2
 * remaining range units. Return 0 when spreading can continue.
 */
s32 move_init_source_panel(s32 direction) {
    move_pathfind_scratch_t* config = g_move_config_ptr;
    u8* frontier = g_move_frontier_flags_ptr;
    move_spread_state_t* state = g_move_scratch_pad_ptr;
    move_record_t* records = g_move_records_ptr;

    if (direction >= 8 || state->source_unit_record_flag != 0) {
        s32 panel_index;
        state->source_unit_record_index = state->current_panel->unit_record_index;
        if (direction >= 8) {
            /* Typed pointer addition reverses the target's address operands. */
            move_record_t* record = (move_record_t*)(state->source_unit_record_index * sizeof(*records) + (u32)records);
            state->source_exit_height = record->standing_height + direction / 4 - 2;
            state->work_height = state->source_base_height_times_two
                + state->source_half_height * ((state->source_slope >> state->source_side_shift) & 3);
            if (state->source_exit_height < state->work_height)
                return 1;
        } else {
            if (!(frontier[state->tile_index] & FRONTIER_FLAG_STAND_ON_UNIT))
                return 2;
            state->source_exit_height = records[state->source_unit_record_index].top_height;
        }
        state->source_side_height_delta = 0;
        state->source_opposite_height = state->source_exit_height;
        state->source_ceiling_height = move_calculate_tile_ceiling(
            state->tile_x, state->tile_y, state->source_side_shift, (state->source_exit_height - 1));
        panel_index = state->source_unit_record_index + MAP_TILE_COUNT;
        state->source_panel_index = panel_index;
        state->source_panel = &g_target_panels[panel_index];
    } else {
        state->source_exit_height = state->source_base_height_times_two
            + state->source_half_height * ((state->source_slope >> state->source_side_shift) & 3);
        state->source_opposite_height = state->source_base_height_times_two
            + state->source_half_height * ((state->source_slope >> state->destination_side_shift) & 3);
        state->source_side_height_delta = state->source_exit_height - state->source_opposite_height;
        state->source_ceiling_height = move_calculate_tile_ceiling(
            state->tile_x, state->tile_y, state->source_side_shift, state->source_exit_height);
        if (state->source_side_height_delta >= 0 && (frontier[state->tile_index] & FRONTIER_FLAG_STEEP))
            return 3;
        if (state->current_tile->depth_half_height.depth
            && (config->movement_set_3 & (BATTLE_MOVEMENT_SET_3_MOVE_IN_WATER | BATTLE_MOVEMENT_SET_3_WALK_ON_WATER))) {
            state->source_exit_height += state->current_tile->depth_half_height.depth * 2;
            if (config->movement_set_3 & BATTLE_MOVEMENT_SET_3_WALK_ON_WATER)
                state->source_exit_height -= 2;
        }
        state->source_panel = &g_target_panels[state->source_panel_index = state->tile_index];
    }
    return (state->source_panel->remaining_range < 2) * 4;
}
