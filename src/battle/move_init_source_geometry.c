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
 * Prepare source-side heights and ceiling for a movement candidate.
 *
 * Return 1 when the source geometry rejects the candidate, otherwise 0.
 * Extended unit records and ordinary terrain use different height checks;
 * only the terrain path adds water depth after finding the ceiling.
 */
s32 move_init_source_geometry(s32 direction) {
    move_spread_state_t* state = g_move_scratch_pad_ptr;
    u8* frontier = g_move_frontier_flags_ptr;
    move_record_t* records = g_move_records_ptr;
    s32 difference;

    if (state->outer_count >= 5) {
        /* Typed pointer addition reverses the target's address operands. */
        move_record_t* record = (move_record_t*)(state->source_unit_record_index * sizeof(*records) + (u32)records);
        state->source_exit_height = record->standing_height + g_move_height_offsets[6 - direction / 4];
        state->work_height = state->source_base_height_times_two
            + state->source_half_height * ((state->source_slope >> state->source_side_shift) & 3);
        if (state->source_exit_height < state->work_height)
            return 1;
        state->source_ceiling_height = move_calculate_tile_ceiling(state->tile_x, state->tile_y,
            state->source_side_shift, (records[state->source_unit_record_index].standing_height + 5));
        state->source_side_height_delta = 0;
    } else if (state->source_unit_record_flag != 0) {
        if (!(frontier[state->tile_index] & FRONTIER_FLAG_STAND_ON_UNIT))
            return 1;
        state->source_exit_height = records[state->source_unit_record_index].top_height;
        state->source_opposite_height = state->source_exit_height;
        state->source_ceiling_height = move_calculate_tile_ceiling(
            state->tile_x, state->tile_y, state->source_side_shift, (state->source_exit_height - 1));
        state->source_side_height_delta = 0;
    } else {
        state->source_exit_height = state->source_base_height_times_two
            + state->source_half_height * ((state->source_slope >> state->source_side_shift) & 3);
        state->source_opposite_height = state->source_base_height_times_two
            + state->source_half_height * ((state->source_slope >> state->destination_side_shift) & 3);
        state->source_ceiling_height = move_calculate_tile_ceiling(
            state->tile_x, state->tile_y, state->source_side_shift, state->source_exit_height);
        difference = state->source_opposite_height - state->source_exit_height;
        state->source_side_height_delta = difference;
        if (difference > 0 && (frontier[state->tile_index] & FRONTIER_FLAG_STEEP))
            return 1;
        state->source_exit_height += state->current_tile->depth_half_height.depth * 2;
    }
    return 0;
}
