#include "fft/battle.h"
#include "fft/battle_ai.h"
#include "fft/battle_move.h"
#include "fft/map.h"

/* Return 1 when the acting unit cannot jump across tile_index at height.
 *
 * Checks both elevation layers, each for its terrain (a blocked frontier tile
 * or the tile's vertical span from its lower side minus ceiling depth to its
 * higher side plus depth) and for a unit standing on it (flag 0x40 records).
 * A span that overlaps the unit's body (height .. height + unit size) blocks
 * the jump; otherwise the jump is blocked only when the lowest span start is
 * above height. shift_a/shift_b select the exit and entry side slope bits.
 *
 * Matching: the a>b arm stages the stored ceiling bound in `high` with the
 * depth term loaded before the store, and the other arm's split `low`
 * assignment keeps `low` ahead of `high` in allocation priority. */
s32 battle_move_check_horizontal_jump(s32 tile_index, u8 shift_a, u8 shift_b, u8 height) {
    battle_move_spread_state_t* state;
    battle_move_record_t* records;
    u8* frontier;
    map_tile_t* tile;
    s32 index;
    s32 layer;
    s32 low;
    u8 high;
    s32 lowest;
    s32 height_a;
    s32 height_b;
    s32 depth;

    lowest = 0x7f;
    frontier = g_battle_move_frontier_flags_ptr;
    state = g_battle_move_scratch_pad_ptr;
    records = g_battle_move_records_ptr;
    state->jump_unit_size = g_battle_move_config_ptr->unit_size;
    for (layer = 0; layer < 4; layer++) {
        index = (layer / 2) * 256 + tile_index;
        tile = &g_battle_map_tile_data[index];
        if (layer & 1) {
            if ((frontier[index] & 0xc) != 4) {
                continue;
            }
            state->work_height = g_battle_target_panels[index].unit_record_index;
            if (!(records[state->work_height].unit_id_flags & 0x40)) {
                continue;
            }
            low = records[state->work_height].standing_height;
            high = low + 3;
            if (height < (u8)high && (u8)high < height + state->jump_unit_size) {
                return 1;
            }
            high = records[state->work_height].top_height;
        } else {
            if (frontier[index] & 0x80) {
                if (frontier[index] & 4) {
                    return 1;
                }
                continue;
            }
            state->work_height = tile->height * 2;
            state->jump_half_height = tile->depth_half_height & 0x1f;
            state->jump_slope = tile->slope_type;
            height_a = state->work_height + state->jump_half_height * ((state->jump_slope >> shift_a) & 3);
            height_b = state->work_height + state->jump_half_height * ((state->jump_slope >> shift_b) & 3);
            if ((u8)height_a > (u8)height_b) {
                high = height_b - (tile->ceiling_depth_and_marks & 0x1f) * 2;
                depth = (tile->depth_half_height >> 5) * 2;
                state->work_height = high;
                low = state->work_height;
                high = height_a + depth;
            } else {
                high = height_b + (tile->depth_half_height >> 5) * 2;
                low = height_a;
                low -= (tile->ceiling_depth_and_marks & 0x1f) * 2;
            }
        }
        if ((u8)low < (u8)lowest) {
            lowest = low;
        }
        if (height < (u8)low && (u8)low < height + state->jump_unit_size) {
            return 1;
        }
        if (height < (u8)high && (u8)high < height + state->jump_unit_size) {
            return 1;
        }
        if ((u8)low <= height && height + state->jump_unit_size <= (u8)high) {
            return 1;
        }
    }
    if ((u8)lowest != 0x7f) {
        if (height < (u8)lowest) {
            return 1;
        }
    }
    return 0;
}
