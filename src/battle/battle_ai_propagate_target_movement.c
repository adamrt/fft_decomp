#include "fft/battle.h"
#include "fft/battle_ai.h"
#include "fft/battle_move.h"

/* Propagate the acting unit's movement budget outward from a target tile.
 *
 * Each pass snapshots the frontier before visiting it. A time-budget pause
 * saves scratchpad state and sets *suspended to 1 while returning 0.
 * Return -1 for an absent unit on initialization, or the reachable count
 * on completion. The caller must check suspension before using that count.
 */
s32 battle_ai_propagate_target_movement(
    s32 unit_id, s32 target_x, s32 target_y, s32 target_elevation, s32 initialize, s32* suspended, s32 check_budget) {
    u8 frontier_snapshot[512];
    battle_move_pathfind_scratch_t* config = g_battle_move_config_ptr;
    battle_move_spread_state_t* state = g_battle_move_scratch_pad_ptr;
    battle_move_record_t* records = g_battle_move_records_ptr;
    u8* frontier = g_battle_move_frontier_flags_ptr;
    s32 completed_pass = 0;
    s32 pass;
    s32 index;
    battle_target_panel_t* panel;

    *suspended = 0;
    if (initialize) {
        if (g_battle_unit_stats[unit_id].entd_slot == BATTLE_ENTD_SLOT_NONE)
            return -1;
        battle_move_store_unit_movement_to_scratchpad(unit_id);
        config->move = 124;
        for (pass = 15; pass >= 0; pass--)
            records[pass].unit_id_flags = 0xff;
        battle_move_set_tile_flags_for_pathfinding(1);
        index = (config->high_elevation << 8) + config->y * config->map_max_x + config->x;
        panel = &g_battle_target_panels[index];
        panel->ride_remaining_range = 0;
        panel->remaining_range = 0;
        frontier[index] &= 0xfe;
        state->outer_count = 4;
        state->inner_count = 4;
        panel->remaining_range = 0;
        panel->max_height_delta = 0xff;
        index = (target_elevation << 8) + target_y * config->map_max_x + target_x;
        panel = &g_battle_target_panels[index];
        panel->remaining_range = 125;
        frontier[index] |= 1;
        panel->max_height_delta = 0;
        g_battle_ai_propagation_resume_pass = 0;
        config->ai_propagation_mode = 1;
    } else {
        /* A direct scratchpad address avoids an extra linker-address addiu. */
        main_util_copy_byte_data(g_battle_ai_unit_snapshot_storage, (void*)0x1f800000, 1024);
    }
    if (config->move_type == BATTLE_MOVEMENT_CLASS_TELEPORT) {
        /* Teleport and Teleport 2 bypass propagation, not destination filtering. */
        u8 remaining_range = 1;
        battle_target_panel_t* fill_panels = g_battle_target_panels;
        for (pass = 511; pass >= 0; pass--)
            fill_panels[pass].remaining_range = remaining_range;
    } else {
        for (pass = g_battle_ai_propagation_resume_pass; pass < config->move; pass++) {
            if (check_budget && battle_ai_is_vsync_hblank_past_threshold() && completed_pass) {
                g_battle_ai_propagation_resume_pass = pass;
                *suspended = 1;
                main_util_copy_byte_data((void*)0x1f800000, g_battle_ai_unit_snapshot_storage, 1024);
                return 0;
            }
            if (!state->frontier_max_remaining_range)
                break;
            for (state->tile_level = 0; state->tile_level < 2; state->tile_level++) {
                state->level_offset = state->tile_level * 256;
                for (state->tile_y = 0; (s16)state->tile_y < config->map_max_y; state->tile_y++) {
                    state->row_offset = state->level_offset + (s16)state->tile_y * config->map_max_x;
                    for (state->tile_x = 0; (s16)state->tile_x < config->map_max_x; state->tile_x++) {
                        state->tile_index = state->row_offset + state->tile_x;
                        state->current_tile = &g_battle_map_tile_data[state->tile_index];
                        state->current_panel = &g_battle_target_panels[state->tile_index];
                        frontier_snapshot[state->tile_index] = frontier[state->tile_index] & 1;
                        frontier[state->tile_index] &= 0xfe;
                    }
                }
            }
            state->frontier_max_remaining_range = 0;
            for (state->tile_level = 0; state->tile_level < 2; state->tile_level++) {
                state->level_offset = state->tile_level * 256;
                for (state->tile_y = 0; (s16)state->tile_y < config->map_max_y; state->tile_y++) {
                    state->row_offset = state->level_offset + (s16)state->tile_y * config->map_max_x;
                    for (state->tile_x = 0; (s16)state->tile_x < config->map_max_x; state->tile_x++) {
                        state->tile_index = state->row_offset + state->tile_x;
                        state->current_tile = &g_battle_map_tile_data[state->tile_index];
                        state->current_panel = &g_battle_target_panels[state->tile_index];
                        if (frontier_snapshot[state->tile_index]) {
                            state->source_unit_record_flag = 0;
                            battle_move_spread_to_neighbors();
                            if (!config->fly_or_teleport) {
                                state->source_unit_record_flag = 0;
                                battle_move_calculate_spread();
                            }
                        }
                    }
                }
            }
            completed_pass = 1;
        }
    }
    battle_move_set_reachable_tile_flags();
    g_battle_move_reachable_tiles_valid = 1;
    g_battle_move_reachable_unit_id = config->unit_id;
    return state->reachable_count;
}
