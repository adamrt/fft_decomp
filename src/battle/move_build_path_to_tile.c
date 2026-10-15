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

/* Build the acting unit's movement path to a target tile.
 *
 * Returns 0 when the target is not marked reachable, when the unit is not the
 * one the reachable-tile map was computed for, or when an occupied end tile
 * is not a valid destination. Teleport movers get the explicit-destination
 * form (0xfe, x, y, elevation) after the teleport roll, or 0xff on failure.
 * Other movers trace remaining-range values back from the target toward the
 * unit before move_encode_path_steps builds the step list. Either way the result is
 * the path buffer at g_move_path.
 *
 * The flag byte at 0x8018f86d is written through a pointer loaded twice: the
 * target materialises its address in a register in both blocks. */
battle_walk_path_t* move_build_path_to_tile(s32 unit_id, s32 x, s32 y, s32 elevation) {
    move_height_offsets_t no_offsets = { 0 };
    move_pathfind_scratch_t* config = g_move_config_ptr;
    move_spread_state_t* state = g_move_scratch_pad_ptr;
    move_record_t* records = g_move_records_ptr;
    u8* frontier = g_move_frontier_flags_ptr;
    s32 index;
    u8 flags;
    u8 source_flags;
    u8 value;
    u8 range;
    u8* move_flags;
    u8 pass;
    s16 previous;
    s16 current;

    config->ai_propagation_mode = 0;
    state->tile_x = config->target_x = x;
    state->tile_y = config->target_y = y;
    state->tile_level = config->target_level = elevation;
    move_store_unit_movement_to_scratchpad(unit_id);
    index = (config->target_level << 8) + config->target_y * config->map_max_x + config->target_x;
    if (!g_map_tile_data[index].ceiling_depth_and_marks.move_destination) {
        return 0;
    }
    if (g_move_reachable_tiles_valid == 0) {
        return 0;
    }
    if (unit_id != g_move_reachable_unit_id) {
        return 0;
    }
    if (g_battle_unit_stats[unit_id].entd_slot == BATTLE_ENTD_SLOT_NONE) {
        return 0;
    }
    move_set_tile_flags_for_pathfinding(MOVE_TILE_FLAGS_MODE_KEEP_RANGES);
    g_move_path_height_offsets = no_offsets;
    g_move_destination_unit_record = 0;
    config->source_tile_occupied = 0;
    config->destination_tile_occupied = 0;
    state->source_unit_record_flag = 0;
    state->source_unit_record_index = 0;
    state->outer_count = 4;
    index = (config->target_level << 8) + config->target_y * config->map_max_x + config->target_x;
    flags = frontier[index];
    if (flags & FRONTIER_FLAG_UNIT_ON_TILE) {
        if (!(flags & FRONTIER_FLAG_MOUNTABLE_UNIT)) {
            return 0;
        }
        config->destination_tile_occupied = 1;
        state->source_unit_record_index = g_target_panels[index].unit_record_index;
        g_move_destination_unit_record
            = (records[state->source_unit_record_index].unit_id_flags & MOVE_RECORD_UNIT_ID_MASK) + 0x80;
        state->outer_count = 0x1c;
    }
    index = (config->high_elevation << 8) + config->y * config->map_max_x + config->x;
    source_flags = frontier[index];
    if (source_flags & FRONTIER_FLAG_UNIT_ON_TILE) {
        if (!(source_flags & FRONTIER_FLAG_MOUNTABLE_UNIT)) {
            return 0;
        }
        config->source_tile_occupied = 1;
    }
    if (config->move_type == MOVE_CLASS_TELEPORT) {
        move_flags = &g_move_effective_flags;
        *move_flags = BATTLE_EFFECTIVE_MOVEMENT_TELEPORT;
        value = config->movement_3;
        if (value & BATTLE_MOVEMENT_SET_3_MOVE_IN_WATER) {
            *move_flags = BATTLE_EFFECTIVE_MOVEMENT_TELEPORT | BATTLE_EFFECTIVE_MOVEMENT_ON_WATER;
        } else if (value & BATTLE_MOVEMENT_SET_3_WALK_ON_WATER) {
            *move_flags = BATTLE_EFFECTIVE_MOVEMENT_TELEPORT | BATTLE_EFFECTIVE_MOVEMENT_IN_WATER;
        }
        if (value & BATTLE_MOVEMENT_SET_3_SILENT_WALK) {
            move_flags = &g_move_effective_flags;
            *move_flags |= BATTLE_EFFECTIVE_MOVEMENT_SILENT_WALK;
        }
        if (move_calculate_teleport_chances() != 0) {
            g_move_path.count = MOVE_PATH_TELEPORT;
            g_move_path.data.teleport.x = x;
            g_move_path.data.teleport.y = y;
            g_move_path.data.teleport.elevation = elevation;
        } else {
            g_move_path.count = MOVE_PATH_TELEPORT_FAILED;
        }
        return &g_move_path;
    }
    state->candidate_remaining_range = 0xff;
    state->path_length = 0;
    state->inner_count = 4;
    state->selected_source_side_shift = 0;
    while (state->candidate_remaining_range != 0) {
        state->previous_source_side_shift = state->selected_source_side_shift;
        pass = state->path_length + 1;
        state->tile_index = state->tile_level * MAP_TILES_PER_LAYER + state->tile_y * config->map_max_x + state->tile_x;
        state->current_tile = &g_map_tile_data[state->tile_index];
        state->current_panel = &g_target_panels[state->tile_index];
        state->path_length = pass;
        if (state->outer_count >= 5) {
            value = state->current_panel->ride_remaining_range;
            range = state->current_panel->remaining_range;
            if (range < value) {
                g_target_panels[state->source_unit_record_index + MAP_TILE_COUNT].ride_remaining_range = pass;
            } else {
                value = range;
                state->source_unit_record_flag = 0;
                state->outer_count = 4;
                state->current_panel->mark = state->path_length;
            }
        } else if (state->source_unit_record_flag != 0) {
            value = g_target_panels[state->source_unit_record_index + MAP_TILE_COUNT].remaining_range;
            g_target_panels[state->source_unit_record_index + MAP_TILE_COUNT].mark = pass;
        } else {
            value = state->current_panel->remaining_range;
            state->current_panel->mark = pass;
        }
        state->candidate_remaining_range = value;
        if (state->tile_x == config->x && state->tile_y == config->y && state->tile_level == config->high_elevation) {
            break;
        }
        move_spread_to_neighbors();
        previous = state->candidate_remaining_range;
        if (previous != value) {
            state->outer_count = 4;
            continue;
        }
        move_calculate_spread();
        current = state->candidate_remaining_range;
        if (current != previous) {
            state->outer_count = 4;
            continue;
        }
        if (config->source_tile_occupied != 0) {
            state->inner_count = 7;
            move_spread_to_neighbors();
            previous = state->candidate_remaining_range;
            if (previous != current) {
                continue;
            }
            move_calculate_spread();
            if (state->candidate_remaining_range != previous) {
                continue;
            }
        }
        return 0;
    }
    move_encode_path_steps();
    return &g_move_path;
}
