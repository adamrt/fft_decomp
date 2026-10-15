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

/* Pathfinding pass for a generic unit (id 0x7f) from (x, y, level) to the target
 * tile; returns the route buffer, or 0 when either tile is invalid or unreachable.
 *
 * The 0xff record fill is an s32 local: a literal fill becomes a single-set
 * hoisted pseudo that sched1 places after `li s2,0xf`.
 */
battle_walk_path_t* move_calculate_pathing(s32 flags, s32 jump, s32 x, s32 y, u32 level, s32 target_x, s32 target_y,
    u32 target_level, s32 initialize, s32* suspended, s32 check_budget) {
    u8 frontier_snapshot[MAP_TILE_COUNT];
    move_height_offsets_t no_offsets = { 0 };
    move_pathfind_scratch_t* config = g_move_config_ptr;
    move_spread_state_t* state = g_move_scratch_pad_ptr;
    move_record_t* records = g_move_records_ptr;
    u8* frontier = g_move_frontier_flags_ptr;
    s32 completed_pass = 0;
    s32 i;
    map_tile_t* tile;
    s32 empty_flags;

    *suspended = 0;
    if (initialize) {
        if (x < 0 || x >= g_map_max_x || y < 0 || y >= g_map_max_y || level >= MAP_TERRAIN_LAYER_COUNT || target_x < 0
            || target_x >= g_map_max_x || target_y < 0 || target_y >= g_map_max_y
            || target_level >= MAP_TERRAIN_LAYER_COUNT) {
            return 0;
        }
        config->ai_propagation_mode = 0;
        config->x = x;
        config->y = y;
        config->high_elevation = level;
        config->target_x = target_x;
        config->target_y = target_y;
        config->target_level = target_level;
        config->map_max_x = g_map_max_x;
        config->map_max_y = g_map_max_y;
        g_move_pathing_tile_index = (config->high_elevation << 8) + config->y * config->map_max_x + config->x;
        if (g_map_tile_data[g_move_pathing_tile_index].flags.blocked
            || g_map_tile_data[g_move_pathing_tile_index].flags.untargetable) {
            return 0;
        }
        config->target_index = (config->target_level << 8) + config->target_y * config->map_max_x + config->target_x;
        if (g_map_tile_data[config->target_index].flags.blocked
            || g_map_tile_data[config->target_index].flags.untargetable) {
            return 0;
        }
        config->unit_id = MOVE_GENERIC_UNIT_ID;
        config->will_drown = 1;
        config->will_sink = 1;
        config->fly_or_teleport = 0;
        config->unit_size = 6;
        config->movement_set_3 = 0;
        g_move_weather_terrain_cost = move_get_weather_terrain_cost();
        config->cost_table = MAIN_TERRAIN_COST_TABLE_DEFAULT;
        config->move_type = MOVE_CLASS_NORMAL;
        config->movement_3 = 0;
        g_move_pathing_ignore_terrain = 0;
        g_move_pathing_scratch = config->cost_table;
        for (i = 0; i < 256; i++) {
            g_move_terrain_cost = g_main_terrain_movement_cost_tables[g_move_pathing_scratch][i];
            if (g_move_terrain_cost == MAIN_TERRAIN_COST_WEATHER) {
                g_move_terrain_cost = g_move_weather_terrain_cost;
            }
            if ((flags & MOVE_PATHFIND_IGNORE_TERRAIN_COST) && g_move_pathing_scratch != 0xFF) {
                g_move_terrain_cost = 1;
            }
            g_move_terrain_costs_ptr[i] = g_move_terrain_cost;
        }
        g_move_pathing_scratch = jump;
        if (g_move_pathing_scratch >= 8) {
            g_move_pathing_scratch = 7;
        }
        config->jump_times_two = g_move_pathing_scratch * 2;
        config->jump_or_1f = g_move_pathing_scratch;
        config->jump_half = g_move_pathing_scratch >> 1;
        config->mountable_chocobo = 0;
        config->move = MOVE_PATHFIND_RANGE;
        config->can_ride = 0;
        g_move_pathing_team = 0;
        config->_unknown_1a = g_battle_unknown_8018f4fc;
        empty_flags = MOVE_RECORD_NONE;
        for (i = MOVE_RECORD_COUNT - 1; i >= 0; i--) {
            records[i].unit_id_flags = empty_flags;
        }
        g_move_pathing_unit_record_count = 0;
        move_set_tile_flags_for_pathfinding(MOVE_TILE_FLAGS_MODE_TERRAIN_ONLY);
        state->outer_count = 4;
        g_target_panels[g_move_pathing_tile_index].remaining_range = config->move + 1;
        g_target_panels[g_move_pathing_tile_index].max_height_delta = 0;
        state->inner_count = 2;
        frontier[g_move_pathing_tile_index] |= FRONTIER_FLAG_QUEUED;
        state->frontier_max_remaining_range = config->move + 1;
        g_move_pathing_resume_pass = 0;
    } else {
        mem_copy_byte_data_source_first(g_ai_unit_snapshot_storage, getScratchAddr(0), PSX_SCRATCHPAD_BYTE_COUNT);
    }
    for (i = g_move_pathing_resume_pass; i < config->move; i++) {
        if (check_budget && ai_is_pathfinding_budget_exhausted() && completed_pass) {
            g_move_pathing_resume_pass = i;
            *suspended = 1;
            mem_copy_byte_data_source_first(getScratchAddr(0), g_ai_unit_snapshot_storage, PSX_SCRATCHPAD_BYTE_COUNT);
            return &g_move_path;
        }
        if (!state->frontier_max_remaining_range) {
            break;
        }
        if (g_target_panels[config->target_index].remaining_range >= state->frontier_max_remaining_range) {
            break;
        }
        for (state->tile_level = 0; state->tile_level < MAP_TERRAIN_LAYER_COUNT; state->tile_level++) {
            state->level_offset = state->tile_level * MAP_TILES_PER_LAYER;
            for (state->tile_y = 0; state->tile_y < config->map_max_y; state->tile_y++) {
                state->row_offset = state->level_offset + state->tile_y * config->map_max_x;
                for (state->tile_x = 0; state->tile_x < config->map_max_x; state->tile_x++) {
                    state->tile_index = state->row_offset + state->tile_x;
                    state->current_tile = &g_map_tile_data[state->tile_index];
                    state->current_panel = &g_target_panels[state->tile_index];
                    frontier_snapshot[state->tile_index] = frontier[state->tile_index] & FRONTIER_FLAG_QUEUED;
                    frontier[state->tile_index] &= ~FRONTIER_FLAG_QUEUED;
                }
            }
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
                    if (frontier_snapshot[state->tile_index]) {
                        move_check_and_spread_one_tile_around();
                        if (!config->fly_or_teleport) {
                            move_spread_horizontal_jump();
                        }
                    }
                }
            }
        }
        completed_pass = 1;
    }
    for (i = 0; i < MAP_TILE_COUNT; i++) {
        tile = &g_map_tile_data[i];
        tile->ceiling_depth_and_marks.move_destination = 0;
        if (g_target_panels[i].ride_remaining_range != 0
            || ((g_map_tile_data[i].depth_half_height.half_height < 3 || !(frontier[i] & FRONTIER_FLAG_LOW_CORNER))
                && g_target_panels[i].remaining_range != 0 && tile->depth_half_height.half_height < 4
                && g_map_tile_data[i].surface.type != MAP_SURFACE_OBSTACLE
                && (tile->depth_half_height.depth == 0 || !config->cannot_stay_on_water)
                && (tile->depth_half_height.depth != 3 || !config->will_drown)
                && (tile->depth_half_height.depth < 4 || !config->will_sink)
                && (tile->surface.type != MAP_SURFACE_LAVA || config->move_type != MOVE_CLASS_FLY)
                && (frontier[i] & FRONTIER_FLAG_ENTERABLE))) {
            state->reachable_count++;
            tile->ceiling_depth_and_marks.move_destination = 1;
        }
    }
    g_move_reachable_tiles_valid = 1;
    g_move_reachable_unit_id = config->unit_id;
    state->tile_x = config->target_x;
    state->tile_y = config->target_y;
    state->tile_level = config->target_level;
    state->selected_source_side_shift = 0;
    g_move_pathing_tile_index = (config->target_level << 8) + config->target_y * config->map_max_x + config->target_x;
    if (!(g_map_tile_data[g_move_pathing_tile_index].ceiling_depth_and_marks.move_destination)) {
        return 0;
    }
    g_move_pathing_frontier = frontier;
    target_clear_panel_spread_flags();
    g_move_path_height_offsets = no_offsets;
    g_move_destination_unit_record = 0;
    config->source_tile_occupied = 0;
    config->destination_tile_occupied = 0;
    state->outer_count = 4;
    state->candidate_remaining_range = 0xFF;
    state->source_unit_record_flag = 0;
    state->source_unit_record_index = 0;
    state->path_length = 0;
    state->inner_count = 2;
    while (state->candidate_remaining_range != 0) {
        state->previous_source_side_shift = state->selected_source_side_shift;
        state->tile_index = state->tile_level * MAP_TILES_PER_LAYER + state->tile_y * config->map_max_x + state->tile_x;
        state->current_tile = &g_map_tile_data[state->tile_index];
        state->current_panel = &g_target_panels[state->tile_index];
        state->path_length++;
        g_move_pathing_scratch = state->current_panel->remaining_range;
        state->current_panel->mark = state->path_length;
        state->candidate_remaining_range = g_move_pathing_scratch;
        if (state->tile_x == config->x && state->tile_y == config->y && state->tile_level == config->high_elevation) {
            break;
        }
        move_spread_to_neighbors();
        if (state->candidate_remaining_range == g_move_pathing_scratch) {
            move_calculate_spread();
            if (state->candidate_remaining_range == g_move_pathing_scratch) {
                return 0;
            }
        }
    }
    move_encode_path_steps();
    return &g_move_path;
}
