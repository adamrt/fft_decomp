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

/* Rank and save a movement candidate for the common finalizer.
 *
 * Every attempt clears budget_matches; accepted candidates replace the saved
 * coordinates, selectors and predecessor budget, then set candidate_saved.
 */
void move_save_selected_candidate(s32 direction, s32 candidate, s32 extra_span) {
    move_spread_state_t* state = g_move_scratch_pad_ptr;
    u8 height_delta;

    state->budget_matches = 0;
    if (state->inner_count >= 5) {
        state->saved_remaining_range = state->effective_destination_panel->remaining_range;
    } else {
        state->work_height = state->effective_destination_panel->max_height_delta;
        if (g_move_config_ptr->move_type == MOVE_CLASS_FLY) {
            if (state->best_fly_height_delta != 0xff) {
                if (state->selected_source_side_shift == state->previous_source_side_shift
                    && state->source_side_shift != state->previous_source_side_shift)
                    return;
                if ((state->selected_source_side_shift == state->previous_source_side_shift
                        || state->source_side_shift != state->previous_source_side_shift)
                    && state->best_fly_height_delta <= state->height_delta)
                    return;
            }
            height_delta = state->height_delta;
            state->height_delta = 0;
            state->best_fly_height_delta = height_delta;
        } else {
            if (state->best_extra_span < extra_span)
                return;
            if (state->best_extra_span == extra_span) {
                if (state->best_height_delta < state->work_height)
                    return;
                if (state->best_height_delta == state->work_height
                    && state->previous_source_side_shift != state->source_side_shift)
                    return;
            }
            if (state->work_height > state->height_delta)
                state->height_delta = state->work_height;
            state->best_extra_span = extra_span;
        }
        state->best_height_delta = state->height_delta;
        state->selected_source_side_shift = state->source_side_shift;
        state->saved_remaining_range = state->effective_destination_panel->remaining_range;
    }
    state->saved_height_offset_flags = 0;
    state->saved_x = state->work_x;
    state->saved_y = state->work_y;
    state->saved_level = state->work_level;
    /* Extended candidates carry source/destination height selectors. */
    if (state->outer_count >= 5) {
        state->saved_height_offset_flags = MOVE_SAVED_SOURCE_HEIGHT;
        state->saved_source_height_offset = g_move_height_offsets[6 - direction / 4];
    }
    if (state->inner_count >= 5) {
        state->saved_height_offset_flags |= MOVE_SAVED_DESTINATION_HEIGHT;
        state->saved_destination_height_offset = g_move_height_offsets[candidate];
    }
    if (state->destination_unit_record_flag != 0) {
        state->saved_unit_record_flag = 1;
        state->saved_unit_record_index = state->destination_unit_record_index;
    } else {
        state->saved_unit_record_flag = 0;
        state->saved_unit_record_index = 0;
    }
    state->candidate_saved = 1;
}
