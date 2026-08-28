#include "fft/battle.h"
#include "fft/battle_move.h"
#include "fft/map.h"
#include "psx/types.h"

/* Raises the destination panel's ride_remaining_range to the remaining movement budget
 * when a movement record offers a reachable standing height.
 *
 * Candidate heights run from the record's standing height (or the destination
 * surface, if higher) up to its top height. The walk stops at the first height
 * the source or destination ceiling rules out, or a climb that is too high or
 * follows an extra span; a drop beyond the jump budget tries the next height.
 *
 * `height` first carries the record index, as the target keeps both roles in
 * one register. The budget stays an s32 read through u8 casts: a u8 local is
 * born one RTL copy later, and that shorter live range outranks extra_span in
 * global allocation, swapping their registers. */
void battle_move_check_occupied_tile_standing_height(s32 record_index, s32 extra_span) {
    battle_move_spread_state_t* state;
    battle_move_pathfind_scratch_t* config;
    battle_move_record_t* records;
    battle_target_panel_t* panel;
    s32 step_cost;
    s32 remaining;
    u8 height;
    u8 value;
    u8 limit;
    u8 top;

    height = record_index;
    state = g_battle_move_scratch_pad_ptr;
    config = g_battle_move_config_ptr;
    records = g_battle_move_records_ptr;

    if (state->destination_ceiling_height < state->source_exit_height + config->unit_size) {
        return;
    }
    panel = state->source_panel;
    step_cost = extra_span + 1;
    remaining = panel->remaining_range - step_cost;
    if ((u8)remaining == 0) {
        return;
    }
    if ((u8)remaining < panel->ride_remaining_range) {
        return;
    }
    top = state->destination_base_height_times_two
        + state->destination_half_height * ((state->destination_slope >> state->destination_side_shift) & 3)
        + (state->destination_tile->depth_half_height >> MAP_TILE_DEPTH_SHIFT) * 2;
    value = records[height].standing_height;
    limit = records[height].standing_height + records[height].body_height;
    if (value < top) {
        value = top;
    }
    for (height = value; height <= limit; height++) {
        if (state->source_ceiling_height < height + config->unit_size) {
            return;
        }
        if (state->destination_ceiling_height < height + config->unit_size) {
            return;
        }
        if (state->source_exit_height < height) {
            if (extra_span != 0) {
                return;
            }
            if (config->jump_times_two < (u8)(height - state->source_exit_height)) {
                return;
            }
        } else if (config->jump_times_two < (u8)(state->source_exit_height - height)) {
            continue;
        }
        if (state->destination_panel->ride_remaining_range < (u8)remaining) {
            state->destination_panel->ride_remaining_range = remaining;
        }
        return;
    }
}
