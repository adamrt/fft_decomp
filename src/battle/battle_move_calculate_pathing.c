#include "fft/battle.h"
#include "fft/battle_ai.h"
#include "fft/battle_move.h"
#include "fft/main_heap.h"
#include "fft/main_runtime.h"
#include "fft/map.h"
#include "psx/types.h"

/* map_tile_t with byte 3 split into its half-height and depth bitfields. */
typedef struct {
    map_tile_surface_t surface;
    u8 _unknown01;
    u8 height;
    struct {
        u8 half_height : 5;
        u8 depth : 3;
    } bits;
    u8 slope_type;
    u8 marks;
    u8 flags_06;
    u8 camera_block_masks;
} battle_move_path_tile_view_t;

/* g_battle_map_tile_data through the bitfield view, bound separately: this
 * function also addresses the map_tile_t array, and a cast of it lets GCC
 * share the two bases. */
extern battle_move_path_tile_view_t g_battle_map_tile_data_view[512];

/* Pathfinding pass for a generic unit (id 0x7f) from (x, y, level) to the target
 * tile; returns the route buffer, or 0 when either tile is invalid or unreachable.
 *
 * The 0xff record fill is an s32 local: a literal fill becomes a single-set
 * hoisted pseudo that sched1 places after `li s2,0xf`.
 */
u8* battle_move_calculate_pathing(s32 flags, s32 jump, s32 x, s32 y, u32 level, s32 target_x, s32 target_y,
    u32 target_level, s32 initialize, s32* suspended, s32 check_budget) {
    u8 frontier_snapshot[512];
    battle_move_pathfind_scratch_t* config = g_battle_move_config_ptr;
    battle_move_spread_state_t* state = g_battle_move_scratch_pad_ptr;
    battle_move_record_t* records = g_battle_move_records_ptr;
    u8* frontier = g_battle_move_frontier_flags_ptr;
    s32 completed_pass = 0;
    s32 i;
    battle_move_path_tile_view_t* tile;
    s32 empty_flags;

    *suspended = 0;
    if (initialize) {
        if (x < 0 || x >= g_battle_map_max_x || y < 0 || y >= g_battle_map_max_y || level >= 2 || target_x < 0
            || target_x >= g_battle_map_max_x || target_y < 0 || target_y >= g_battle_map_max_y || target_level >= 2) {
            return 0;
        }
        config->ai_propagation_mode = 0;
        config->x = x;
        config->y = y;
        config->high_elevation = level;
        config->target_x = target_x;
        config->target_y = target_y;
        config->_pad0c = target_level;
        config->map_max_x = g_battle_map_max_x;
        config->map_max_y = g_battle_map_max_y;
        g_battle_move_pathing_tile_index = (config->high_elevation << 8) + config->y * config->map_max_x + config->x;
        if (g_battle_map_tile_data[g_battle_move_pathing_tile_index].flags_06.bits.blocked
            || g_battle_map_tile_data[g_battle_move_pathing_tile_index].flags_06.bits.untargetable) {
            return 0;
        }
        *(u16*)config = (config->_pad0c << 8) + config->target_y * config->map_max_x + config->target_x;
        if (g_battle_map_tile_data[*(u16*)config].flags_06.bits.blocked
            || g_battle_map_tile_data[*(u16*)config].flags_06.bits.untargetable) {
            return 0;
        }
        config->unit_id = 0x7F;
        config->will_drown = 1;
        config->will_sink = 1;
        config->fly_or_teleport = 0;
        config->unit_size = 6;
        config->movement_set_3 = 0;
        g_battle_move_weather_severity = battle_map_get_weather_severity();
        config->move_mod = 0;
        config->move_type = BATTLE_MOVEMENT_CLASS_NORMAL;
        config->movement_3 = 0;
        D_8018F7D0 = 0;
        g_battle_move_pathing_scratch = config->move_mod;
        for (i = 0; i < 256; i++) {
            g_battle_move_terrain_cost = g_main_terrain_movement_cost_tables[g_battle_move_pathing_scratch][i];
            if (g_battle_move_terrain_cost == 0) {
                g_battle_move_terrain_cost = g_battle_move_weather_severity;
            }
            if ((flags & 0x100) && g_battle_move_pathing_scratch != 0xFF) {
                g_battle_move_terrain_cost = 1;
            }
            g_battle_move_terrain_costs_ptr[i] = g_battle_move_terrain_cost;
        }
        g_battle_move_pathing_scratch = jump;
        if (g_battle_move_pathing_scratch >= 8) {
            g_battle_move_pathing_scratch = 7;
        }
        config->jump_times_two = g_battle_move_pathing_scratch * 2;
        config->jump_or_1f = g_battle_move_pathing_scratch;
        config->jump_half = g_battle_move_pathing_scratch >> 1;
        config->mountable_chocobo = 0;
        config->move = 124;
        config->can_ride = 0;
        D_8018F7CC = 0;
        config->field_1a = D_8018F4FC;
        empty_flags = 0xFF;
        for (i = 15; i >= 0; i--) {
            records[i].unit_id_flags = empty_flags;
        }
        D_8018F7D8 = 0;
        battle_move_set_tile_flags_for_pathfinding(3);
        state->outer_count = 4;
        g_battle_target_panels[g_battle_move_pathing_tile_index].remaining_range = config->move + 1;
        g_battle_target_panels[g_battle_move_pathing_tile_index].max_height_delta = 0;
        state->inner_count = 2;
        frontier[g_battle_move_pathing_tile_index] |= 1;
        state->frontier_max_remaining_range = config->move + 1;
        g_battle_move_pathing_resume_pass = 0;
    } else {
        main_util_copy_byte_data(g_battle_ai_unit_snapshot_storage, (void*)0x1f800000, 1024);
    }
    for (i = g_battle_move_pathing_resume_pass; i < config->move; i++) {
        if (check_budget && battle_ai_is_vsync_hblank_past_threshold() && completed_pass) {
            g_battle_move_pathing_resume_pass = i;
            *suspended = 1;
            main_util_copy_byte_data((void*)0x1f800000, g_battle_ai_unit_snapshot_storage, 1024);
            return g_battle_move_path;
        }
        if (!state->frontier_max_remaining_range) {
            break;
        }
        if (g_battle_target_panels[*(u16*)config].remaining_range >= state->frontier_max_remaining_range) {
            break;
        }
        for (state->tile_level = 0; state->tile_level < 2; state->tile_level++) {
            state->level_offset = state->tile_level * 256;
            for (state->tile_y = 0; (s16)state->tile_y < config->map_max_y; state->tile_y++) {
                state->row_offset = state->level_offset + (s16)state->tile_y * config->map_max_x;
                for (state->tile_x = 0; (s16)state->tile_x < config->map_max_x; state->tile_x++) {
                    state->tile_index = state->row_offset + state->tile_x;
                    state->current_tile = &g_battle_map_tile_data[state->tile_index];
                    state->current_panel = &g_battle_target_panels[state->tile_index];
                    frontier_snapshot[state->tile_index] = frontier[state->tile_index] & 1;
                    frontier[state->tile_index] &= 0xFE;
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
                        battle_move_check_and_spread_one_tile_around();
                        if (!config->fly_or_teleport) {
                            battle_move_spread_horizontal_jump();
                        }
                    }
                }
            }
        }
        completed_pass = 1;
    }
    for (i = 0; i < 512; i++) {
        tile = &g_battle_map_tile_data_view[i];
        tile->marks &= 0xDF;
        if (g_battle_target_panels[i].ride_remaining_range != 0
            || ((g_battle_map_tile_data_view[i].bits.half_height < 3 || !(frontier[i] & 2))
                && g_battle_target_panels[i].remaining_range != 0 && tile->bits.half_height < 4
                && g_battle_map_tile_data_view[i].surface.bits.type != 0x1C
                && (tile->bits.depth == 0 || !config->cannot_stay_on_water)
                && (tile->bits.depth != 3 || !config->will_drown) && (tile->bits.depth < 4 || !config->will_sink)
                && (tile->surface.bits.type != 0x12 || config->move_type != 0) && (frontier[i] & 0x10))) {
            state->reachable_count++;
            tile->marks |= 0x20;
        }
    }
    g_battle_move_reachable_tiles_valid = 1;
    g_battle_move_reachable_unit_id = config->unit_id;
    state->tile_x = config->target_x;
    state->tile_y = config->target_y;
    state->tile_level = config->_pad0c;
    state->selected_source_side_shift = 0;
    g_battle_move_pathing_tile_index = (config->_pad0c << 8) + config->target_y * config->map_max_x + config->target_x;
    if (!((g_battle_map_tile_data[g_battle_move_pathing_tile_index].ceiling_depth_and_marks >> 5) & 1)) {
        return 0;
    }
    g_battle_move_pathing_frontier = frontier;
    battle_target_clear_panel_spread_flags();
    g_battle_move_path_height_offsets = 0;
    g_battle_move_destination_unit_record = 0;
    config->_pad1e[0] = 0;
    config->_pad1e[1] = 0;
    state->outer_count = 4;
    state->candidate_remaining_range.value = 0xFF;
    state->source_unit_record_flag = 0;
    state->source_unit_record_index = 0;
    state->_pad5c[2] = 0;
    state->inner_count = 2;
    while (state->candidate_remaining_range.value != 0) {
        state->previous_source_side_shift = state->selected_source_side_shift;
        state->tile_index = state->tile_level * 256 + (s16)state->tile_y * config->map_max_x + state->tile_x;
        state->current_tile = &g_battle_map_tile_data[state->tile_index];
        state->current_panel = &g_battle_target_panels[state->tile_index];
        state->_pad5c[2]++;
        g_battle_move_pathing_scratch = state->current_panel->remaining_range;
        state->current_panel->mark = state->_pad5c[2];
        state->candidate_remaining_range.value = g_battle_move_pathing_scratch;
        if ((s16)state->tile_x == config->x && (s16)state->tile_y == config->y
            && state->tile_level == config->high_elevation) {
            break;
        }
        battle_move_spread_to_neighbors();
        if (state->candidate_remaining_range.value == g_battle_move_pathing_scratch) {
            battle_move_calculate_spread();
            if (state->candidate_remaining_range.value == g_battle_move_pathing_scratch) {
                return 0;
            }
        }
    }
    battle_move_encode_path_steps();
    return g_battle_move_path;
}
