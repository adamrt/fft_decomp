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
 * Build the player-side movement range for a unit: reset the pathfinding
 * scratchpad, then spread the movement budget outward one pass per remaining
 * move point, visiting every frontier tile on both levels.  Teleport skips
 * propagation and marks the whole grid reachable.  Returns -1 for an absent
 * unit, otherwise the reachable tile count.
 *
 * The AI twin ai_propagate_target_movement (0x80178ca4) does the same
 * with a suspendable time budget and a snapshot of the frontier; this one
 * clears frontier_max_remaining_range up front and visits the live frontier
 * directly, so it needs only one set of nested level/y/x loops.
 * The callers also pass map coordinates in a1-a3; the target callee does not
 * read them, but retaining the parameters preserves the original calls.
 */
s32 move_set_reachable_tiles(s32 unit_id, s32 unused_map_x, s32 unused_map_y, s32 unused_map_z) {
    move_pathfind_scratch_t* config = g_move_config_ptr;
    move_spread_state_t* state = g_move_scratch_pad_ptr;
    u8* frontier = g_move_frontier_flags_ptr;
    s32 pass;

    if (g_battle_unit_stats[unit_id].entd_slot == BATTLE_ENTD_SLOT_NONE) {
        return -1;
    }
    move_store_unit_movement_to_scratchpad(unit_id);
    move_set_tile_flags_for_pathfinding(MOVE_TILE_FLAGS_MODE_START_SPREAD);
    if (config->move_type == MOVE_CLASS_TELEPORT) {
        u8 remaining_range = 1;
        target_panel_t* fill_panels = g_target_panels;
        for (pass = 511; pass >= 0; pass--) {
            fill_panels[pass].remaining_range = remaining_range;
        }
    } else {
        for (pass = 0; pass < config->move; pass++) {
            if (!state->frontier_max_remaining_range) {
                break;
            }
            state->frontier_max_remaining_range = 0;
            for (state->tile_level = 0; state->tile_level < MAP_TERRAIN_LAYER_COUNT; state->tile_level++) {
                state->level_offset = state->tile_level * MAP_TILES_PER_LAYER;
                for (state->tile_y = 0; state->tile_y < config->map_max_y; state->tile_y++) {
                    state->row_offset = state->level_offset + state->tile_y * config->map_max_x;
                    for (state->tile_x = 0; state->tile_x < config->map_max_x; state->tile_x++) {
                        state->tile_index = state->row_offset + state->tile_x;
                        state->current_tile = &g_map_tile_data[state->tile_index];
                        state->current_panel = &g_target_panels[state->tile_index];
                        if (frontier[state->tile_index] & FRONTIER_FLAG_QUEUED) {
                            frontier[state->tile_index] &= ~FRONTIER_FLAG_QUEUED;
                            move_check_and_spread_one_tile_around();
                            if (!config->fly_or_teleport) {
                                move_spread_horizontal_jump();
                            }
                            state->outer_count = 8;
                        }
                    }
                }
            }
        }
    }
    move_set_reachable_tile_flags();
    g_move_reachable_tiles_valid = 1;
    g_move_reachable_unit_id = config->unit_id;
    return state->reachable_count;
}
