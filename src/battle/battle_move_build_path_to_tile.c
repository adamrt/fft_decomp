#include "fft/battle.h"
#include "psx/types.h"

/* Build the acting unit's movement path to a target tile.
 *
 * Returns 0 when the target is not marked reachable, when the unit is not the
 * one the reachable-tile map was computed for, or when an occupied end tile
 * is not a valid destination. Teleport movers get the explicit-destination
 * form (0xfe, x, y, elevation) after the teleport roll, or 0xff on failure.
 * Other movers trace remaining-range values back from the target toward the
 * unit before battle_move_encode_path_steps builds the step list. Either way the result is
 * the path buffer at g_battle_move_path.
 *
 * The flag byte at 0x8018f86d is written through a pointer loaded twice: the
 * target materialises its address in a register in both blocks. */
u8* battle_move_build_path_to_tile(s32 unit_id, s32 x, s32 y, s32 elevation) {
    battle_move_pathfind_scratch_t* config = g_battle_move_config_ptr;
    battle_move_spread_state_t* state = g_battle_move_scratch_pad_ptr;
    battle_move_record_t* records = g_battle_move_records_ptr;
    u8* frontier = g_battle_move_frontier_flags_ptr;
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
    battle_move_store_unit_movement_to_scratchpad(unit_id);
    index = (config->target_level << 8) + config->target_y * config->map_max_x + config->target_x;
    if (!((g_battle_map_tile_data[index].ceiling_depth_and_marks >> 5) & 1)) {
        return 0;
    }
    if (g_battle_move_reachable_tiles_valid == 0) {
        return 0;
    }
    if (unit_id != g_battle_move_reachable_unit_id) {
        return 0;
    }
    if (g_battle_unit_stats[unit_id].entd_slot == BATTLE_ENTD_SLOT_NONE) {
        return 0;
    }
    battle_move_set_tile_flags_for_pathfinding(2);
    g_battle_move_path_height_offsets = 0;
    g_battle_move_destination_unit_record = 0;
    config->source_tile_occupied = 0;
    config->destination_tile_occupied = 0;
    state->source_unit_record_flag = 0;
    state->source_unit_record_index = 0;
    state->outer_count = 4;
    index = (config->target_level << 8) + config->target_y * config->map_max_x + config->target_x;
    flags = frontier[index];
    if (flags & FRONTIER_FLAG_UNIT_ON_TILE) {
        if (!(flags & FRONTIER_FLAG_VALID_DESTINATION)) {
            return 0;
        }
        config->destination_tile_occupied = 1;
        state->source_unit_record_index = g_battle_target_panels[index].unit_record_index;
        g_battle_move_destination_unit_record = (records[state->source_unit_record_index].unit_id_flags & 0x1f) + 0x80;
        state->outer_count = 0x1c;
    }
    index = (config->high_elevation << 8) + config->y * config->map_max_x + config->x;
    source_flags = frontier[index];
    if (source_flags & FRONTIER_FLAG_UNIT_ON_TILE) {
        if (!(source_flags & FRONTIER_FLAG_VALID_DESTINATION)) {
            return 0;
        }
        config->source_tile_occupied = 1;
    }
    if (config->move_type == 1) {
        move_flags = &g_battle_move_effective_flags;
        *move_flags = 8;
        value = config->movement_3;
        if (value & 0x80) {
            *move_flags = 0x28;
        } else if (value & 0x40) {
            *move_flags = 0x18;
        }
        if (value & 2) {
            move_flags = &g_battle_move_effective_flags;
            *move_flags |= 2;
        }
        if (battle_move_calculate_teleport_chances() != 0) {
            g_battle_move_path[0] = 0xfe;
            g_battle_move_path[1] = x;
            g_battle_move_path[2] = y;
            g_battle_move_path[3] = elevation;
        } else {
            g_battle_move_path[0] = 0xff;
        }
        return g_battle_move_path;
    }
    state->candidate_remaining_range.value = 0xff;
    state->path_length = 0;
    state->inner_count = 4;
    state->selected_source_side_shift = 0;
    while (state->candidate_remaining_range.value != 0) {
        state->previous_source_side_shift = state->selected_source_side_shift;
        pass = state->path_length + 1;
        state->tile_index = state->tile_level * 256 + (s16)state->tile_y * config->map_max_x + state->tile_x;
        state->current_tile = &g_battle_map_tile_data[state->tile_index];
        state->current_panel = &g_battle_target_panels[state->tile_index];
        state->path_length = pass;
        if (state->outer_count >= 5) {
            value = state->current_panel->ride_remaining_range;
            range = state->current_panel->remaining_range;
            if (range < value) {
                g_battle_target_panels[state->source_unit_record_index + 0x200].ride_remaining_range = pass;
            } else {
                value = range;
                state->source_unit_record_flag = 0;
                state->outer_count = 4;
                state->current_panel->mark = state->path_length;
            }
        } else if (state->source_unit_record_flag != 0) {
            value = g_battle_target_panels[state->source_unit_record_index + 0x200].remaining_range;
            g_battle_target_panels[state->source_unit_record_index + 0x200].mark = pass;
        } else {
            value = state->current_panel->remaining_range;
            state->current_panel->mark = pass;
        }
        state->candidate_remaining_range.value = value;
        if ((s16)state->tile_x == config->x && (s16)state->tile_y == config->y
            && state->tile_level == config->high_elevation) {
            break;
        }
        battle_move_spread_to_neighbors();
        previous = state->candidate_remaining_range.value;
        if (previous != value) {
            state->outer_count = 4;
            continue;
        }
        battle_move_calculate_spread();
        current = state->candidate_remaining_range.value;
        if (current != previous) {
            state->outer_count = 4;
            continue;
        }
        if (config->source_tile_occupied != 0) {
            state->inner_count = 7;
            battle_move_spread_to_neighbors();
            previous = state->candidate_remaining_range.value;
            if (previous != current) {
                continue;
            }
            battle_move_calculate_spread();
            if (state->candidate_remaining_range.value != previous) {
                continue;
            }
        }
        return 0;
    }
    battle_move_encode_path_steps();
    return g_battle_move_path;
}
