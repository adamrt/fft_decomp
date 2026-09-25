#include "fft/battle.h"

/*
 * Apply the saved movement candidate to the current spreading state.
 *
 * Copy the saved coordinates and residual, then restore half-height offsets
 * when their saved presence bits are set. The destination update uses byte
 * masks because its equivalent bitfield assignments change register allocation.
 */
void battle_move_apply_selected_candidate(battle_move_spread_state_t* state) {
    state->candidate_remaining_range.value = state->saved_remaining_range;
    state->tile_x = state->saved_x;
    state->tile_y = state->saved_y;
    state->tile_level = state->saved_level;
    if (state->saved_height_offset_flags & BATTLE_MOVEMENT_SAVED_SOURCE_HEIGHT) {
        u8* offsets = &g_battle_move_path_height_offsets;
        battle_move_height_offsets_t packed;
        packed.byte = *offsets;
        packed.bits.source_height_offset = state->saved_source_height_offset;
        packed.bits.source_present = 1;
        *offsets = packed.byte;
    }
    if (state->saved_height_offset_flags & BATTLE_MOVEMENT_SAVED_DESTINATION_HEIGHT) {
        u8* offsets = &g_battle_move_path_height_offsets;
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
