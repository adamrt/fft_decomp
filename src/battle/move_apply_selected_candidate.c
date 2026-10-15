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
 * Apply the saved movement candidate to the current spreading state.
 *
 * Copy the saved coordinates and residual, then restore half-height offsets
 * when their saved presence bits are set. The destination update uses byte
 * masks because its equivalent bitfield assignments change register allocation.
 */
void move_apply_selected_candidate(move_spread_state_t* state) {
    state->candidate_remaining_range = state->saved_remaining_range;
    state->tile_x = state->saved_x;
    state->tile_y = state->saved_y;
    state->tile_level = state->saved_level;
    if (state->saved_height_offset_flags & MOVE_SAVED_SOURCE_HEIGHT) {
        move_height_offsets_t* offsets = &g_move_path_height_offsets;
        move_height_offsets_t packed = *offsets;
        packed.source_height_offset = state->saved_source_height_offset;
        packed.source_present = 1;
        *offsets = packed;
    }
    if (state->saved_height_offset_flags & MOVE_SAVED_DESTINATION_HEIGHT) {
        u8* offsets = (u8*)&g_move_path_height_offsets;
        u32 selected = state->saved_destination_height_offset;
        u32 packed = *offsets;
        selected &= 7;
        selected <<= 1;
        packed &= 0xf1;
        *offsets = packed | selected | 1;
    }
    state->source_unit_record_flag = state->saved_unit_record_flag;
    state->source_unit_record_index = state->saved_unit_record_index;
}
