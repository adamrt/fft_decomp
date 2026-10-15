/* Reachable tiles, spread, jumps and path building. */

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

/* Weather severity, the difficulty of moving through weather-affected tiles:
 * 1 for none/rain/snow, 2 for a storm or snowstorm, 3 for a strong storm or
 * snowstorm. */
s32 move_get_weather_terrain_cost(void) {
    s32 weather;

    weather = map_get_effective_weather();
    if (weather < BATTLE_WEATHER_STORM || weather == BATTLE_WEATHER_SNOW) {
        return 1;
    }
    if (weather == BATTLE_WEATHER_STORM || weather == BATTLE_WEATHER_SNOWSTORM) {
        return 2;
    }
    return 3;
}

/* Whether a unit is a chocobo that can be ridden: BATTLE_CHOCOBO_MOUNT_NONE for
 * any other job or a chocobo with a status of the unmountable set, PAIRED when
 * its mount_info is set, else FREE. */
battle_chocobo_mount_state_e battle_unit_get_chocobo_mount_state(const unit_stats_t* unit) {
    if (unit->job_id < JOB_ID_CHOCOBO || unit->job_id > JOB_ID_RED_CHOCOBO)
        return BATTLE_CHOCOBO_MOUNT_NONE;
    if (main_unit_has_status_in_set(unit, MAIN_STATUS_CHECK_SET_UNMOUNTABLE) != 0)
        return BATTLE_CHOCOBO_MOUNT_NONE;
    if (unit->mount_info != 0)
        return BATTLE_CHOCOBO_MOUNT_PAIRED;
    return BATTLE_CHOCOBO_MOUNT_FREE;
}

/* Fills the pathfinding scratch pad for `unit_id`: its movement, jump and
 * movement-type parameters at 0x1f800000, the per-surface terrain costs, and
 * one seven-byte record per other active unit at 0x1f800080.
 *
 * `value` and `byte` are reused for several unrelated fields; the target keeps
 * each in one register, which decides the allocation of `other` and the
 * standing-height arithmetic.
 *
 * `mount` and `blocked` hold the 0xFF record fill and terrain-cost sentinel,
 * and the cost table base is loaded before its row offset is added: literal
 * constants and `g_main_terrain_movement_cost_tables[cost_table]` let sched1 reorder the pre-loop
 * `li s2`/`li a3`/`lbu` sequence.
 */
void move_store_unit_movement_to_scratchpad(s32 unit_id) {
    move_pathfind_scratch_t* config;
    move_record_t* record;
    move_record_t* current;
    unit_stats_t* unit;
    unit_stats_t* other;
    map_tile_t* tile;
    /* Byte access retains the original packed load and field-extraction order. */
    u8 movement[3];
    battle_unit_height_data_t height_data;
    s32 i;
    s32 count;
    s32 weather;
    s32 ignore_terrain;
    const u8* table;
    u8 value;
    u8 movement_3;
    u8 team;
    u8 mount;
    u8 byte;
    s32 base;
    s32 adjust;
    s32 depth;
    s32 blocked;

    config = g_move_config_ptr;
    record = g_move_records_ptr;
    battle_unit_calculate_height_data(&height_data, unit_id);
    unit = &g_battle_unit_stats[unit_id];
    for (i = 0; i < 3; i++) {
        movement[i] = unit->ability_flags.movement[i];
    }
    config->unit_id = unit_id;
    config->x = unit->x;
    config->y = unit->position.y;
    config->high_elevation = unit->position.higher_elevation;
    config->map_max_x = g_map_max_x;
    config->map_max_y = g_map_max_y;
    config->cannot_enter_water = 0;
    config->cannot_stay_on_water = 0;
    config->will_drown = 1;
    config->will_sink = 1;
    config->fly_or_teleport = 0;
    config->stepping_stone = unit->position.stepping_stone;
    if (movement[2] & (BATTLE_MOVEMENT_SET_3_MOVE_ON_LAVA | BATTLE_MOVEMENT_SET_3_FLOAT)) {
        config->can_pass_lava = 1;
    } else {
        config->can_pass_lava = 0;
    }
    value = unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHICKEN)];
    if (value & (STATUS_BYTE_MASK(STATUS_ID_CHICKEN) | STATUS_BYTE_MASK(STATUS_ID_FROG))) {
        movement[1] &= ~(BATTLE_MOVEMENT_SET_2_TELEPORT_2 | BATTLE_MOVEMENT_SET_2_TELEPORT);
        movement[2] &= ~BATTLE_MOVEMENT_SET_3_FLY;
        config->unit_size = 4;
    } else {
        config->unit_size = 6;
    }
    if (!(movement[2] & (BATTLE_MOVEMENT_SET_3_MOVE_IN_WATER | BATTLE_MOVEMENT_SET_3_FLOAT))
        && (movement[1] & BATTLE_MOVEMENT_SET_2_CANNOT_ENTER_WATER)) {
        config->cannot_stay_on_water = 1;
    }
    if (movement[2] & BATTLE_MOVEMENT_SET_3_FLOAT) {
        config->unit_size += 2;
        config->movement_set_3 = movement[2] | BATTLE_MOVEMENT_SET_3_MOVE_IN_WATER;
    } else {
        config->movement_set_3 = movement[2];
    }
    if (value & STATUS_BYTE_MASK(STATUS_ID_FLOAT)) {
        movement[2] |= BATTLE_MOVEMENT_SET_3_FLOAT;
    }
    if (movement[1] & BATTLE_MOVEMENT_SET_2_CANNOT_ENTER_WATER) {
        movement[2] &= ~(BATTLE_MOVEMENT_SET_3_MOVE_UNDERWATER | BATTLE_MOVEMENT_SET_3_WALK_ON_WATER);
    }
    weather = move_get_weather_terrain_cost();
    if (movement[2] & BATTLE_MOVEMENT_SET_3_FLY) {
        config->cost_table = MAIN_TERRAIN_COST_TABLE_FLY_TELEPORT;
        config->move_type = MOVE_CLASS_FLY;
        config->fly_or_teleport = 1;
    } else if (movement[1] & (BATTLE_MOVEMENT_SET_2_TELEPORT | BATTLE_MOVEMENT_SET_2_TELEPORT_2)) {
        config->cost_table = MAIN_TERRAIN_COST_TABLE_FLY_TELEPORT;
        config->movement_set_2 = movement[1];
        config->move_type = MOVE_CLASS_TELEPORT;
        config->fly_or_teleport = 1;
    } else {
        if (movement[2] & BATTLE_MOVEMENT_SET_3_FLOAT) {
            config->cost_table = MAIN_TERRAIN_COST_TABLE_FLOAT;
            config->move_type = MOVE_CLASS_FLOAT;
        } else if (movement[2] & BATTLE_MOVEMENT_SET_3_MOVE_ON_LAVA) {
            config->cost_table = MAIN_TERRAIN_COST_TABLE_LAVA;
            config->move_type = MOVE_CLASS_LAVA;
        } else if (movement[2] & BATTLE_MOVEMENT_SET_3_MOVE_IN_WATER) {
            config->cost_table = MAIN_TERRAIN_COST_TABLE_WATER;
            config->move_type = MOVE_CLASS_WATER_SURFACE;
        } else if (movement[2] & BATTLE_MOVEMENT_SET_3_WALK_ON_WATER) {
            config->cost_table = MAIN_TERRAIN_COST_TABLE_WATER;
            config->move_type = MOVE_CLASS_WATER_DEPTH_ONE;
        } else if (movement[2] & BATTLE_MOVEMENT_SET_3_MOVE_UNDERWATER) {
            config->cost_table = MAIN_TERRAIN_COST_TABLE_UNDERWATER;
            config->move_type = MOVE_CLASS_UNDERWATER;
        } else {
            config->cost_table = MAIN_TERRAIN_COST_TABLE_DEFAULT;
            config->move_type = MOVE_CLASS_NORMAL;
        }
    }
    if (config->movement_set_3 & (BATTLE_MOVEMENT_SET_3_MOVE_IN_WATER | BATTLE_MOVEMENT_SET_3_WALK_ON_WATER)) {
        config->will_sink = 0;
    }
    if (config->movement_set_3
        & (BATTLE_MOVEMENT_SET_3_MOVE_IN_WATER | BATTLE_MOVEMENT_SET_3_WALK_ON_WATER
            | BATTLE_MOVEMENT_SET_3_MOVE_UNDERWATER)) {
        config->will_drown = 0;
    }
    if (!(movement[2] & (BATTLE_MOVEMENT_SET_3_MOVE_IN_WATER | BATTLE_MOVEMENT_SET_3_FLOAT | BATTLE_MOVEMENT_SET_3_FLY))
        && (movement[1] & BATTLE_MOVEMENT_SET_2_CANNOT_ENTER_WATER)) {
        config->cannot_enter_water = 1;
    }
    config->movement_3 = movement[2];
    if (movement[1] & BATTLE_MOVEMENT_SET_2_ANY_WEATHER) {
        weather = 1;
    }
    ignore_terrain = movement[1] & BATTLE_MOVEMENT_SET_2_ANY_GROUND;
    i = 0;
    blocked = MAIN_TERRAIN_COST_BLOCKED;
    table = g_main_terrain_movement_cost_tables[0];
    table += config->cost_table * 64;
    for (; i < 0x40; i++) {
        byte = table[i];
        if (byte == MAIN_TERRAIN_COST_WEATHER) {
            byte = weather;
        } else if (ignore_terrain && byte != blocked) {
            byte = 1;
        }
        g_move_terrain_costs_ptr[i] = byte;
    }
    value = unit->jump;
    if (value >= 8) {
        value = 7;
    }
    config->jump_half = value >> 1;
    if (config->move_type == MOVE_CLASS_FLY || (movement[0] & BATTLE_MOVEMENT_SET_1_IGNORE_HEIGHT)) {
        value = 0x1F;
    }
    config->jump_times_two = (value & 0xFF) * 2;
    config->jump_or_1f = value;
    if (battle_unit_get_chocobo_mount_state(unit) != BATTLE_CHOCOBO_MOUNT_NONE) {
        config->mountable_chocobo = 1;
    } else {
        config->mountable_chocobo = 0;
    }
    value = unit->move;
    if (value >= 0x7D) {
        value = 0x7C;
    }
    config->move = value;
    if ((unit->unit_flags & (UNIT_FLAG_MALE | UNIT_FLAG_FEMALE))
        && !(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHICKEN)]
            & (STATUS_BYTE_MASK(STATUS_ID_CHICKEN) | STATUS_BYTE_MASK(STATUS_ID_FROG)))
        && !(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHARM)] & STATUS_BYTE_MASK(STATUS_ID_CHARM))) {
        config->can_ride = 1;
    } else {
        config->can_ride = 0;
    }
    team = unit->initial_team_flags;
    config->_unknown_1a = g_battle_unknown_8018f4fc;
    mount = MOVE_RECORD_NONE;
    for (i = MOVE_RECORD_COUNT - 1; i >= 0; i--) {
        record[i].unit_id_flags = mount;
    }
    count = 0;
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        if (count >= MOVE_RECORD_COUNT) {
            break;
        }
        other = &g_battle_unit_stats[i];
        if (other->entd_slot == BATTLE_ENTD_SLOT_NONE) {
            continue;
        }
        if (other->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)]
            & (STATUS_BYTE_MASK(STATUS_ID_CRYSTAL) | STATUS_BYTE_MASK(STATUS_ID_DEAD)
                | STATUS_BYTE_MASK(STATUS_ID_JUMP))) {
            continue;
        }
        if (other->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_TREASURE)] & STATUS_BYTE_MASK(STATUS_ID_TREASURE)) {
            continue;
        }
        if (i == unit_id) {
            continue;
        }
        mount = other->mount_info;
        if ((mount & BATTLE_MOUNT_INFO_FLAG_RIDER) && (mount & BATTLE_MOUNT_INFO_PARTNER_ID_MASK) == config->unit_id) {
            config->unit_size += 2;
            continue;
        }
        value = i;
        if (battle_unit_get_chocobo_mount_state(other) != BATTLE_CHOCOBO_MOUNT_FREE
            && other->mount_info != (config->unit_id | BATTLE_MOUNT_INFO_FLAG_MOUNT)) {
            value = i | MOVE_RECORD_FLAG_NOT_MOUNTABLE;
        }
        byte = other->team_flags;
        if ((team ^ byte) & BATTLE_TEAM_MASK) {
            value |= MOVE_RECORD_FLAG_OTHER_TEAM;
        }
        current = &record[count];
        current->unit_id_flags = value;
        current->x = other->x;
        current->y = other->position.y;
        current->higher_elevation = other->position.higher_elevation;
        current->stepping_stone = other->position.stepping_stone;
        value = other->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHICKEN)];
        if (value & (STATUS_BYTE_MASK(STATUS_ID_CHICKEN) | STATUS_BYTE_MASK(STATUS_ID_FROG))) {
            current->body_height = 4;
            current->stepping_stone = 0;
        } else {
            current->body_height = 6;
        }
        value = other->mount_info;
        if ((value & BATTLE_MOUNT_INFO_FLAG_MOUNT) && (value & BATTLE_MOUNT_INFO_PARTNER_ID_MASK) != config->unit_id) {
            current->body_height += 2;
        }
        movement_3 = other->ability_flags.movement[2];
        tile = &g_map_tile_data[map_calculate_location(other)];
        base = tile->height * 2 + ((*(u8*)&tile->depth_half_height) & MAP_TILE_HALF_HEIGHT_MASK);
        depth = (*(u8*)&tile->depth_half_height) >> 5;
        byte = base;
        if (depth != 0) {
            if (other->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FLOAT)] & STATUS_BYTE_MASK(STATUS_ID_FLOAT)) {
                adjust = depth * 2 + 2;
                byte = base + adjust;
            } else if (movement_3 & BATTLE_MOVEMENT_SET_3_MOVE_IN_WATER) {
                adjust = depth * 2;
                byte = base + adjust;
            } else if (movement_3 & BATTLE_MOVEMENT_SET_3_WALK_ON_WATER) {
                adjust = depth * 2 - 2;
                byte = base + adjust;
            }
        } else if (other->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FLOAT)] & STATUS_BYTE_MASK(STATUS_ID_FLOAT)) {
            byte = base + 2;
        }
        current->standing_height = byte;
        current->top_height = byte + current->body_height;
        count++;
    }
}

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

/* Clears the enterable flag on the tile of every dead or jumping unit. */
void move_clear_enterable_flags_under_dead_or_jumping_units(void) {
    s32 unit_id;

    for (unit_id = 0; unit_id < BATTLE_UNIT_SLOT_COUNT; unit_id++) {
        unit_stats_t* unit = &g_battle_unit_stats[unit_id];

        if ((unit->entd_slot != BATTLE_ENTD_SLOT_NONE)
            && ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DEAD)]
                    & (STATUS_BYTE_MASK(STATUS_ID_DEAD) | STATUS_BYTE_MASK(STATUS_ID_JUMP)))
                != 0)) {
            g_move_frontier_flags_ptr[map_calculate_location(unit)] &= ~FRONTIER_FLAG_ENTERABLE;
        }
    }
}

/* Rebuild the frontier flags of all terrain tiles from slope, half height, depth
 * and the tile flags, then mark the tiles under the 16 unit records. A unit of
 * another team makes its tile not enterable (a flier may still cross on top of
 * it). One of the mover's team can be mounted unless its record says
 * otherwise, and stood on when it is a stepping stone and the mover is not
 * one. Altima's second form blocks its tile.
 *
 * A mode with bit 0 set also clears the panel ranges and the reachable count.
 * START_SPREAD queues the mover's own tile with Move + 1 range (and the unit
 * record on that tile when it is mountable); TERRAIN_ONLY leaves the units
 * out. */
void move_set_tile_flags_for_pathfinding(s32 mode) {
    move_pathfind_scratch_t* config = g_move_config_ptr;
    move_spread_state_t* state = g_move_scratch_pad_ptr;
    u8* frontier = g_move_frontier_flags_ptr;
    move_record_t* records = g_move_records_ptr;
    move_record_t* record;
    target_panel_t* panel;
    target_panel_t* aux;
    map_tile_t* tile;
    s32 i;
    s32 index;
    s32 height;
    u8* frontier_cell;
    u8* unit_cell;
    u8 slope;
    map_tile_flags_t tile_flags;
    u8 flags;
    u8 depth;
    u8 value;

    for (i = 0, frontier_cell = frontier; i < MOVE_PANEL_COUNT; frontier_cell++, i++) {
        panel = &g_target_panels[i];
        panel->mark = 0;
        panel->unit_record_index = 0;
        if (mode & 1) {
            panel->remaining_range = 0;
            panel->ride_remaining_range = 0;
            panel->max_height_delta = 0xff;
        }
        if (i < MAP_TILE_COUNT) {
            tile = &g_map_tile_data[i];
            frontier[i] = 0;
            slope = tile->slope_type & 5;
            if (slope == 1 || slope == 4) {
                *frontier_cell |= FRONTIER_FLAG_LOW_CORNER;
                if (tile->depth_half_height.half_height >= 3) {
                    *frontier_cell |= FRONTIER_FLAG_STEEP;
                }
            } else {
                if (tile->depth_half_height.half_height > config->jump_or_1f) {
                    *frontier_cell |= FRONTIER_FLAG_STEEP;
                }
            }
            if ((tile->flags.blocked | tile->flags.untargetable) == 0) {
                depth = tile->depth_half_height.depth;
                if ((depth == 0 || !config->cannot_enter_water) && (depth < 4 || !config->will_sink)) {
                    *frontier_cell |= FRONTIER_FLAG_ENTERABLE;
                }
            }
            tile_flags = tile->flags;
            if (tile_flags.blocked) {
                *frontier_cell |= FRONTIER_FLAG_BLOCKED;
            } else if (!config->move_type && (tile_flags.untargetable)
                && (tile->surface.type) != MAP_SURFACE_CROSS_SECTION) {
                *frontier_cell |= FRONTIER_FLAG_ENTERABLE;
            }
        }
    }
    if (mode != MOVE_TILE_FLAGS_MODE_TERRAIN_ONLY) {
        for (i = 0; i < MOVE_RECORD_COUNT; i++) {
            record = &records[i];
            flags = record->unit_id_flags;
            if (flags != MOVE_RECORD_NONE) {
                index = (record->higher_elevation << 8) + record->y * config->map_max_x + record->x;
                if (g_battle_unit_stats[flags & MOVE_RECORD_UNIT_ID_MASK].character_identity
                    == CHARACTER_IDENTITY_ALTIMA_SECOND_FORM) {
                    frontier[index] = (frontier[index] | FRONTIER_FLAG_BLOCKED) & ~FRONTIER_FLAG_ENTERABLE;
                }
                g_target_panels[index].unit_record_index = i;
                unit_cell = &frontier[index];
                *unit_cell |= FRONTIER_FLAG_UNIT_ON_TILE;
                if (!(flags & MOVE_RECORD_FLAG_OTHER_TEAM)) {
                    if (!config->stepping_stone) {
                        height = record->stepping_stone << 3;
                        *unit_cell = (*unit_cell & ~FRONTIER_FLAG_STAND_ON_UNIT) | height;
                    } else {
                        *unit_cell &= ~FRONTIER_FLAG_STAND_ON_UNIT;
                    }
                    if (!(flags & MOVE_RECORD_FLAG_NOT_MOUNTABLE)) {
                        frontier[index] |= FRONTIER_FLAG_MOUNTABLE_UNIT;
                    } else {
                        frontier[index] &= ~FRONTIER_FLAG_MOUNTABLE_UNIT;
                    }
                } else {
                    *unit_cell &= ~(FRONTIER_FLAG_ENTERABLE | FRONTIER_FLAG_MOUNTABLE_UNIT);
                    if (!config->move_type) {
                        *unit_cell |= FRONTIER_FLAG_STAND_ON_UNIT;
                    } else {
                        *unit_cell &= ~FRONTIER_FLAG_STAND_ON_UNIT;
                    }
                }
            }
        }
    }
    if (mode & 1) {
        state->reachable_count = 0;
        index = (config->high_elevation << 8) + config->y * config->map_max_x + config->x;
    }
    if (mode == MOVE_TILE_FLAGS_MODE_START_SPREAD) {
        panel = &g_target_panels[index];
        if (frontier[index] & FRONTIER_FLAG_MOUNTABLE_UNIT) {
            state->outer_count = 0x24;
            aux = g_target_panels + MAP_TILE_COUNT + panel->unit_record_index;
            aux->remaining_range = config->move + 1;
            aux->max_height_delta = 0;
        } else {
            state->outer_count = 8;
        }
        state->inner_count = 4;
        panel->remaining_range = config->move + 1;
        panel->max_height_delta = 0;
        frontier[index] |= FRONTIER_FLAG_QUEUED;
        state->frontier_max_remaining_range = config->move + 1;
    }
}

/* Mark every tile the acting unit can end its move on and count them.
 *
 * Teleporting units may land on any valid-destination tile and riders on
 * any mountable panel. Otherwise the tile needs remaining range, a low
 * enough slope, a passable surface and depth the unit tolerates, and must be
 * crossable and either empty or a valid destination the unit can ride onto. */
void move_set_reachable_tile_flags(void) {
    move_pathfind_scratch_t* config;
    move_spread_state_t* spread;
    u8* flags;
    s32 self;
    s32 i;
    map_tile_t* tile;
    target_panel_t* panel;
    s32 half_height;
    s32 surface;
    map_tile_flags_t tile_flags;
    map_tile_depth_t water_depth;
    s32 frontier;

    config = g_move_config_ptr;
    spread = g_move_scratch_pad_ptr;
    flags = g_move_frontier_flags_ptr;
    self = (config->high_elevation << 8) + config->y * config->map_max_x + config->x;
    move_clear_enterable_flags_under_dead_or_jumping_units();
    for (i = 0; i < MAP_TILE_COUNT; i++) {
        tile = &g_map_tile_data[i];
        tile->ceiling_depth_and_marks.move_destination = 0;
        panel = &g_target_panels[i];
        if (i == self) {
            continue;
        }
        if (!((config->move_type == MOVE_CLASS_TELEPORT) && (flags[i] & FRONTIER_FLAG_MOUNTABLE_UNIT))
            && !((panel->ride_remaining_range != 0) && (config->can_ride != 0))) {
            half_height = tile->depth_half_height.half_height;
            if ((half_height >= 3) && (flags[i] & FRONTIER_FLAG_LOW_CORNER)) {
                continue;
            }
            if (panel->remaining_range == 0) {
                continue;
            }
            if (half_height >= 4) {
                continue;
            }
            tile_flags = tile->flags;
            if (tile_flags.cannot_stop) {
                continue;
            }
            surface = tile->surface.type;
            if (surface == MAP_SURFACE_OBSTACLE) {
                continue;
            }
            water_depth = tile->depth_half_height;
            if ((water_depth.depth != 0) && (config->cannot_stay_on_water != 0)) {
                continue;
            }
            if ((water_depth.depth == 3) && (config->will_drown != 0)) {
                continue;
            }
            if ((tile->depth_half_height.depth >= 4) && (config->will_sink != 0)) {
                continue;
            }
            if ((surface == MAP_SURFACE_LAVA) && (config->can_pass_lava == 0)) {
                continue;
            }
            if (tile_flags.untargetable) {
                continue;
            }
            frontier = flags[i];
            if (!(frontier & FRONTIER_FLAG_ENTERABLE)) {
                continue;
            }
            if (frontier & FRONTIER_FLAG_UNIT_ON_TILE) {
                if (!(frontier & FRONTIER_FLAG_MOUNTABLE_UNIT) || (config->can_ride == 0)) {
                    continue;
                }
            }
        }
        spread->reachable_count++;
        tile->ceiling_depth_and_marks.move_destination = 1;
    }
}

/* Step +x: the slope shifts select the source's exit side and the
 * destination's entry side. */
void move_spread_to_east_preset(void) {
    move_spread_state_t* spread = g_move_scratch_pad_ptr;
    spread->x_step = 1;
    spread->y_step = 0;
    spread->source_side_shift = 0;
    spread->destination_side_shift = 2;
}

/* Step +y: the slope shifts select the source's exit side and the
 * destination's entry side. */
void move_spread_to_north_preset(void) {
    move_spread_state_t* spread = g_move_scratch_pad_ptr;
    spread->y_step = 1;
    spread->source_side_shift = 6;
    spread->x_step = 0;
    spread->destination_side_shift = 4;
}

/* Step -x: the slope shifts select the source's exit side and the
 * destination's entry side. */
void move_spread_to_west_preset(void) {
    move_spread_state_t* spread = g_move_scratch_pad_ptr;
    spread->x_step = -1;
    spread->y_step = 0;
    spread->source_side_shift = 2;
    spread->destination_side_shift = 0;
}

/* Step -y: the slope shifts select the source's exit side and the
 * destination's entry side. */
void move_spread_to_south_preset(void) {
    move_spread_state_t* spread = g_move_scratch_pad_ptr;
    spread->y_step = -1;
    spread->source_side_shift = 4;
    spread->x_step = 0;
    spread->destination_side_shift = 6;
}

/* Reset the best-candidate fields of the spread state and load the current
 * tile's heights. */
void move_init_spread_scratch(void) {
    move_spread_state_t* spread = g_move_scratch_pad_ptr;
    spread->best_height_delta = 0xFF;
    spread->best_fly_height_delta = 0xFF;
    spread->budget_matches = 0;
    spread->candidate_saved = 0;
    spread->best_extra_span = 0x7F;
    move_transfer_tiles_height_halves_and_slope_to_scratch_pad();
}

/* Load the current tile's base height (doubled, in half-height units), half
 * height and slope into the spread state. */
void move_transfer_tiles_height_halves_and_slope_to_scratch_pad(void) {
    move_spread_state_t* state = g_move_scratch_pad_ptr;
    state->source_base_height_times_two = state->current_tile->height << 1;
    state->source_half_height = state->current_tile->depth_half_height.half_height;
    state->source_slope = state->current_tile->slope_type;
}

/* Work tile = current tile + one step of the preset direction. */
void move_set_spreading_tile_x_and_y_coordinates(void) {
    move_spread_state_t* spread = g_move_scratch_pad_ptr;
    spread->work_x = spread->tile_x + spread->x_step;
    spread->work_y = spread->tile_y + spread->y_step;
}

/*
 * Check whether the spreading coordinates lie outside the map.
 *
 * Return 1 for an invalid coordinate, or 0 for an in-bounds tile.
 */
s32 move_check_spreading_tile_coordinates(void) {
    move_spread_state_t* state = g_move_scratch_pad_ptr;
    move_pathfind_scratch_t* config = g_move_config_ptr;
    s32 x = state->work_x;
    s32 y;
    if (x >= 0) {
        y = state->work_y;
        if (y < 0)
            return 1;
        if (x >= config->map_max_x)
            return 1;
        if (y < config->map_max_y)
            return 0;
    }
    return 1;
}

/*
 * Select a source movement panel and prepare its exit height.
 *
 * Return 1 for a low extended sample, 2 for an unavailable unit-top path,
 * 3 for a restricted terrain exit, or 4 for a panel with less than 2
 * remaining range units. Return 0 when spreading can continue.
 */
s32 move_init_source_panel(s32 direction) {
    move_pathfind_scratch_t* config = g_move_config_ptr;
    u8* frontier = g_move_frontier_flags_ptr;
    move_spread_state_t* state = g_move_scratch_pad_ptr;
    move_record_t* records = g_move_records_ptr;

    if (direction >= 8 || state->source_unit_record_flag != 0) {
        s32 panel_index;
        state->source_unit_record_index = state->current_panel->unit_record_index;
        if (direction >= 8) {
            /* Typed pointer addition reverses the target's address operands. */
            move_record_t* record = (move_record_t*)(state->source_unit_record_index * sizeof(*records) + (u32)records);
            state->source_exit_height = record->standing_height + direction / 4 - 2;
            state->work_height = state->source_base_height_times_two
                + state->source_half_height * ((state->source_slope >> state->source_side_shift) & 3);
            if (state->source_exit_height < state->work_height)
                return 1;
        } else {
            if (!(frontier[state->tile_index] & FRONTIER_FLAG_STAND_ON_UNIT))
                return 2;
            state->source_exit_height = records[state->source_unit_record_index].top_height;
        }
        state->source_side_height_delta = 0;
        state->source_opposite_height = state->source_exit_height;
        state->source_ceiling_height = move_calculate_tile_ceiling(
            state->tile_x, state->tile_y, state->source_side_shift, (state->source_exit_height - 1));
        panel_index = state->source_unit_record_index + MAP_TILE_COUNT;
        state->source_panel_index = panel_index;
        state->source_panel = &g_target_panels[panel_index];
    } else {
        state->source_exit_height = state->source_base_height_times_two
            + state->source_half_height * ((state->source_slope >> state->source_side_shift) & 3);
        state->source_opposite_height = state->source_base_height_times_two
            + state->source_half_height * ((state->source_slope >> state->destination_side_shift) & 3);
        state->source_side_height_delta = state->source_exit_height - state->source_opposite_height;
        state->source_ceiling_height = move_calculate_tile_ceiling(
            state->tile_x, state->tile_y, state->source_side_shift, state->source_exit_height);
        if (state->source_side_height_delta >= 0 && (frontier[state->tile_index] & FRONTIER_FLAG_STEEP))
            return 3;
        if (state->current_tile->depth_half_height.depth
            && (config->movement_set_3 & (BATTLE_MOVEMENT_SET_3_MOVE_IN_WATER | BATTLE_MOVEMENT_SET_3_WALK_ON_WATER))) {
            state->source_exit_height += state->current_tile->depth_half_height.depth * 2;
            if (config->movement_set_3 & BATTLE_MOVEMENT_SET_3_WALK_ON_WATER)
                state->source_exit_height -= 2;
        }
        state->source_panel = &g_target_panels[state->source_panel_index = state->tile_index];
    }
    return (state->source_panel->remaining_range < 2) * 4;
}

/*
 * Check a destination path and propagate an improved movement budget.
 *
 * Return 0 for an accepted path, otherwise a numeric rejection reason.
 * The mount helper can update separate state before a later rejection.
 */
s32 move_propagate_destination(s32 candidate, s32 extra_span) {
    move_spread_state_t* state = g_move_scratch_pad_ptr;
    move_pathfind_scratch_t* config = g_move_config_ptr;
    u8* flags = g_move_frontier_flags_ptr;
    move_record_t* records = g_move_records_ptr;
    u8* terrain_costs = g_move_terrain_costs_ptr;
    u8* destination_flags;

    state->work_level = candidate & 1;
    state->destination_unit_record_flag = candidate / 2;
    state->destination_tile = &g_map_tile_data[state->destination_index
        = state->work_level * MAP_TILES_PER_LAYER + state->work_y * config->map_max_x + state->work_x];
    state->destination_panel = &g_target_panels[state->destination_index];
    state->destination_base_height_times_two = state->destination_tile->height * 2;
    state->destination_half_height = state->destination_tile->depth_half_height.half_height;
    state->destination_slope = state->destination_tile->slope_type;
    destination_flags = &flags[state->destination_index];
    if (state->destination_unit_record_flag != 0) {
        if (!(*destination_flags & (FRONTIER_FLAG_MOUNTABLE_UNIT | FRONTIER_FLAG_STAND_ON_UNIT)))
            return 1; /* no unit there to mount or stand on */
        state->destination_unit_record_index = state->destination_panel->unit_record_index;
        state->destination_entry_height = records[state->destination_unit_record_index].top_height;
        state->destination_ceiling_height = move_calculate_tile_ceiling(
            state->work_x, state->work_y, state->destination_side_shift, (state->destination_entry_height - 1));
        if (*destination_flags & FRONTIER_FLAG_MOUNTABLE_UNIT)
            move_check_occupied_tile_standing_height(state->destination_unit_record_index, extra_span);
        if (!(*destination_flags & FRONTIER_FLAG_STAND_ON_UNIT))
            return 17; /* the unit cannot be stood on (a mount was handled by the helper) */
    } else {
        if (!(*destination_flags & FRONTIER_FLAG_ENTERABLE))
            return 2; /* the tile is not enterable */
        state->destination_entry_height = state->destination_base_height_times_two
            + state->destination_half_height * ((state->destination_slope >> state->destination_side_shift) & 3);
        state->destination_opposite_height = state->destination_base_height_times_two
            + state->destination_half_height * ((state->destination_slope >> state->source_side_shift) & 3);
        state->destination_side_height_delta = state->destination_opposite_height - state->destination_entry_height;
        state->destination_ceiling_height = move_calculate_tile_ceiling(
            state->work_x, state->work_y, state->destination_side_shift, state->destination_entry_height);
        if (state->destination_side_height_delta > 0 && (*destination_flags & FRONTIER_FLAG_STEEP))
            return 3; /* a steep slope entered from its low side */
    }
    if (state->destination_ceiling_height < state->source_exit_height + config->unit_size)
        return 4; /* too low a ceiling over the destination for the mover's exit height */
    if (state->source_ceiling_height < state->destination_entry_height + config->unit_size)
        return 5; /* too low a ceiling over the source for the destination's entry height */
    if (state->destination_ceiling_height < state->destination_entry_height + config->unit_size)
        return 6; /* no headroom on the destination itself */
    if (state->destination_unit_record_flag == 0)
        state->destination_entry_height += state->destination_tile->depth_half_height.depth * 2;
    if (state->destination_entry_height > state->source_exit_height) {
        if (extra_span != 0)
            return 8; /* a step that spans tiles cannot climb */
        state->height_delta = state->destination_entry_height - state->source_exit_height;
    } else {
        state->height_delta = state->source_exit_height - state->destination_entry_height;
    }
    if (state->height_delta > config->jump_times_two)
        return 9; /* beyond the jump limit */
    state->work_height = state->source_panel->max_height_delta;
    state->height_delta = state->work_height < state->height_delta ? state->height_delta : state->work_height;
    if (config->move_type == MOVE_CLASS_FLY)
        state->height_delta = 0;
    if (state->destination_unit_record_flag != 0) {
        state->candidate_remaining_range = state->source_panel->remaining_range - 1 - extra_span;
        if (state->candidate_remaining_range <= 0)
            return 11; /* no range left to step onto the unit */
        if ((u8)state->candidate_remaining_range
            < g_target_panels[state->destination_unit_record_index + MAP_TILE_COUNT].remaining_range)
            return 12; /* the unit top is already reached with more range left */
        if ((u8)state->candidate_remaining_range
                == g_target_panels[state->destination_unit_record_index + MAP_TILE_COUNT].remaining_range
            && state->height_delta
                >= g_target_panels[state->destination_unit_record_index + MAP_TILE_COUNT].max_height_delta)
            return 13; /* same range left, and no smaller height difference */
        g_target_panels[state->destination_unit_record_index + MAP_TILE_COUNT].remaining_range
            = (u8)state->candidate_remaining_range;
        g_target_panels[state->destination_unit_record_index + MAP_TILE_COUNT].max_height_delta = state->height_delta;
    } else {
        state->candidate_remaining_range
            = state->source_panel->remaining_range - extra_span - terrain_costs[state->destination_tile->surface.type];
        if (state->candidate_remaining_range <= 0)
            return 14; /* no range left after the terrain cost */
        if ((u8)state->candidate_remaining_range < state->destination_panel->remaining_range)
            return 15; /* the tile is already reached with more range left */
        if ((u8)state->candidate_remaining_range == state->destination_panel->remaining_range
            && state->height_delta >= state->destination_panel->max_height_delta)
            return 16; /* same range left, and no smaller height difference */
        state->destination_panel->remaining_range = (u8)state->candidate_remaining_range;
        state->destination_panel->max_height_delta = state->height_delta;
    }
    if (state->candidate_remaining_range >= 2) {
        g_move_frontier_flags_ptr[state->destination_index] |= FRONTIER_FLAG_QUEUED;
        if ((u8)state->candidate_remaining_range > state->frontier_max_remaining_range)
            state->frontier_max_remaining_range = (u8)state->candidate_remaining_range;
    }
    return 0;
}

/* Spread one step from the current tile. Source i uses direction i % 4 (east,
 * north, west, south): 0..3 leave from the ground, 4..7 from the top of the
 * unit on the tile, and 8 and up (outer_count 0x24) from that unit's standing
 * height + i / 4 - 2. Each source whose neighbor is on the map tries
 * inner_count destinations: either level, on the ground or on a unit. */
void move_check_and_spread_one_tile_around(void) {
    move_spread_state_t* state;
    s32 i;
    s32 j;

    state = g_move_scratch_pad_ptr;
    move_transfer_tiles_height_halves_and_slope_to_scratch_pad();
    for (i = 0; i < state->outer_count; i++) {
        state->source_unit_record_flag = i >= 4;
        g_move_spread_preset_table[i % 4]();
        move_set_spreading_tile_x_and_y_coordinates();
        if ((move_check_spreading_tile_coordinates() == 0) && (move_init_source_panel(i) == 0)) {
            for (j = 0; j < state->inner_count; j++) {
                move_propagate_destination(j, 0);
            }
        }
    }
}

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
void move_check_occupied_tile_standing_height(s32 record_index, s32 extra_span) {
    move_spread_state_t* state;
    move_pathfind_scratch_t* config;
    move_record_t* records;
    target_panel_t* panel;
    s32 step_cost;
    s32 remaining;
    u8 height;
    u8 value;
    u8 limit;
    u8 top;

    height = record_index;
    state = g_move_scratch_pad_ptr;
    config = g_move_config_ptr;
    records = g_move_records_ptr;

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
        + state->destination_tile->depth_half_height.depth * 2;
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

enum {
    TILE_CEILING_NONE = 0x7f,
};

/*
 * Returns the lowest ceiling above (x, y) across the two elevation layers, or
 * 0x7f when no tile above exit_height forms one.  exit_slope_shift selects the
 * exit side's two slope bits; exit_height is the exit side height (or tile
 * height plus unit size).
 */
u8 move_calculate_tile_ceiling(s32 x, s32 y, s32 exit_slope_shift, u8 exit_height) {
    s32 elevation;
    map_tile_t* tile;
    map_tile_depth_t depth;
    u8 ceiling;
    u8 tile_ceiling;

    ceiling = TILE_CEILING_NONE;
    for (elevation = 0; elevation < MAP_TERRAIN_LAYER_COUNT; elevation++) {
        tile = &g_map_tile_data[elevation * MAP_TILES_PER_LAYER + y * g_move_config_ptr->map_max_x + x];
        if ((tile->flags.blocked) == 0) {
            depth = tile->depth_half_height;
            tile_ceiling = (tile->height << 1) + (depth.half_height) * ((tile->slope_type >> exit_slope_shift) & 3)
                - ((tile->ceiling_depth_and_marks.ceiling_depth) << 1);
            if (tile_ceiling > exit_height) {
                if (ceiling < tile_ceiling) {
                    tile_ceiling = ceiling;
                }
                ceiling = tile_ceiling;
            }
        }
    }
    return ceiling;
}

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
s32 move_check_horizontal_jump(s32 tile_index, u8 shift_a, u8 shift_b, u8 height) {
    move_spread_state_t* state;
    move_record_t* records;
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
    frontier = g_move_frontier_flags_ptr;
    state = g_move_scratch_pad_ptr;
    records = g_move_records_ptr;
    state->jump_unit_size = g_move_config_ptr->unit_size;
    for (layer = 0; layer < 4; layer++) {
        index = (layer / 2) * MAP_TILES_PER_LAYER + tile_index;
        tile = &g_map_tile_data[index];
        if (layer & 1) {
            if ((frontier[index] & (FRONTIER_FLAG_UNIT_ON_TILE | FRONTIER_FLAG_STAND_ON_UNIT))
                != FRONTIER_FLAG_UNIT_ON_TILE) {
                continue;
            }
            state->work_height = g_target_panels[index].unit_record_index;
            if (!(records[state->work_height].unit_id_flags & MOVE_RECORD_FLAG_OTHER_TEAM)) {
                continue;
            }
            low = records[state->work_height].standing_height;
            high = low + 3;
            if (height < high && high < height + state->jump_unit_size) {
                return 1;
            }
            high = records[state->work_height].top_height;
        } else {
            if (frontier[index] & FRONTIER_FLAG_BLOCKED) {
                if (frontier[index] & FRONTIER_FLAG_UNIT_ON_TILE) {
                    return 1;
                }
                continue;
            }
            state->work_height = tile->height * 2;
            state->jump_half_height = tile->depth_half_height.half_height;
            state->jump_slope = tile->slope_type;
            height_a = state->work_height + state->jump_half_height * ((state->jump_slope >> shift_a) & 3);
            height_b = state->work_height + state->jump_half_height * ((state->jump_slope >> shift_b) & 3);
            if ((u8)height_a > (u8)height_b) {
                high = height_b - (tile->ceiling_depth_and_marks.ceiling_depth) * 2;
                depth = (tile->depth_half_height.depth) * 2;
                state->work_height = high;
                low = state->work_height;
                high = height_a + depth;
            } else {
                high = height_b + (tile->depth_half_height.depth) * 2;
                low = height_a;
                low -= (tile->ceiling_depth_and_marks.ceiling_depth) * 2;
            }
        }
        if ((u8)low < (u8)lowest) {
            lowest = low;
        }
        if (height < (u8)low && (u8)low < height + state->jump_unit_size) {
            return 1;
        }
        if (height < high && high < height + state->jump_unit_size) {
            return 1;
        }
        if ((u8)low <= height && height + state->jump_unit_size <= high) {
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

/* Spread movement across horizontal jumps of up to half the acting unit's Jump.
 *
 * The first four directions spread from the tile and the rest from above the
 * unit (source_unit_record_flag). Each pass continues only while the source panel's remaining
 * range minus the jump span stays at least 2 and both spans are clear. */
void move_spread_horizontal_jump(void) {
    move_pathfind_scratch_t* config = g_move_config_ptr;
    move_spread_state_t* state = g_move_scratch_pad_ptr;
    s32 outer_index;
    s32 inner_index;
    s32 pass_index;

    move_transfer_tiles_height_halves_and_slope_to_scratch_pad();
    for (outer_index = 0; outer_index < state->outer_count; outer_index++) {
        state->source_unit_record_flag = outer_index >= 4;
        g_move_spread_preset_table[outer_index % 4]();
        if (move_init_source_panel(outer_index) == 0) {
            move_set_spreading_tile_x_and_y_coordinates();
            for (pass_index = 1; pass_index <= config->jump_half; pass_index++) {
                state->jump_remaining_range = state->source_panel->remaining_range - pass_index;
                if (state->jump_remaining_range < 2) {
                    break;
                }
                if (move_check_spreading_tile_coordinates() != 0) {
                    break;
                }
                if (move_check_horizontal_jump(state->work_y * config->map_max_x + state->work_x,
                        state->source_side_shift, state->destination_side_shift, state->source_exit_height)
                    != 0) {
                    break;
                }
                state->work_x = state->tile_x + state->x_step * (pass_index + 1);
                state->work_y = state->tile_y + state->y_step * (pass_index + 1);
                if (move_check_spreading_tile_coordinates() != 0) {
                    break;
                }
                for (inner_index = 0; inner_index < state->inner_count; inner_index++) {
                    move_propagate_destination(inner_index, pass_index);
                }
            }
        }
    }
}

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

/* 1 when the teleport succeeds: always with Teleport 2, otherwise it fails
 * with a 10 in 100 chance per tile of distance beyond Move. */
s32 move_calculate_teleport_chances(void) {
    move_pathfind_scratch_t* ctx = g_move_config_ptr;
    s32 dx, dy;

    if (ctx->movement_set_2 & BATTLE_MOVEMENT_SET_2_TELEPORT_2) {
        return 1;
    }
    dy = ctx->target_x - ctx->x;
    dx = ctx->target_y - ctx->y;
    if (dy < 0)
        dy = -dy;
    if (dx < 0)
        dx = -dx;
    return main_util_roll_pass_fail(0x64, ((dy + dx) - ctx->move) * 10);
}

/* padding */

/* Encode the traced path as move_step_t bytes in g_move_path,
 * count holding the number of encoded steps. Starting from the unit's tile, step n goes to
 * the tile or unit record the back-trace marked with path_length - n; the byte
 * records the direction, the distance, the level, whether the step ends on a
 * unit and whether either end is a climb. */
void move_encode_path_steps(void) {
    move_pathfind_scratch_t* config = g_move_config_ptr;
    move_spread_state_t* state = g_move_scratch_pad_ptr;
    move_record_t* records = g_move_records_ptr;
    u8* frontier = g_move_frontier_flags_ptr;
    move_record_t* record;
    map_tile_t* tile;
    s32 step;
    s32 path_index;
    u8 level;
    s32 i;
    s32 target;
    s32 rem;
    u8 hit;
    u8 dir;
    s32 flag;
    u8 source_bit;
    u8 destination_bit;

    state->source_unit_record_index = 0;
    for (step = 0; step < state->path_length - 1; step++) {
        state->tile_index = state->tile_level * MAP_TILES_PER_LAYER + state->tile_y * config->map_max_x + state->tile_x;
        state->current_tile = &g_map_tile_data[state->tile_index];
        state->current_panel = &g_target_panels[state->tile_index];
        path_index = step + 1;
        target = state->path_length - path_index;
        for (i = 0; i < MOVE_PANEL_COUNT; i++) {
            if (i >= MAP_TILE_COUNT && g_target_panels[i].ride_remaining_range == (target & 0xff)) {
                hit = 1;
                record = &records[i - MAP_TILE_COUNT];
            } else if (g_target_panels[i].mark == (target & 0xff)) {
                hit = 0;
                if (i < MAP_TILE_COUNT) {
                    state->destination_unit_record_index = 0;
                    state->work_level = (u8)(i / MAP_TILES_PER_LAYER);
                    rem = i - (i / MAP_TILES_PER_LAYER) * MAP_TILES_PER_LAYER;
                    state->work_y = (u8)(rem / config->map_max_x);
                    state->work_x = (u8)(rem % config->map_max_x);
                } else {
                    state->destination_unit_record_index = 1;
                    record = &records[i - MAP_TILE_COUNT];
                }
            } else {
                continue;
            }
            if (state->destination_unit_record_index || hit) {
                state->work_x = record->x;
                state->work_y = record->y;
                state->work_level = record->higher_elevation;
            }
            if (state->work_x == state->tile_x) {
                dir = 0xc0;
                if (state->work_y > state->tile_y) {
                    state->source_side_shift = 6;
                    state->destination_side_shift = 4;
                    state->step_distance = state->work_y - state->tile_y;
                } else {
                    dir = 0x80;
                    state->source_side_shift = 4;
                    state->destination_side_shift = 6;
                    state->step_distance = state->tile_y - state->work_y;
                }
            } else {
                dir = 0;
                if (state->tile_x < state->work_x) {
                    state->source_side_shift = 0;
                    state->destination_side_shift = 2;
                    state->step_distance = state->work_x - state->tile_x;
                } else {
                    dir = 0x40;
                    state->source_side_shift = 2;
                    state->destination_side_shift = 0;
                    state->step_distance = state->tile_x - state->work_x;
                }
            }
            state->destination_climb = 0;
            if (!state->destination_unit_record_index && !hit) {
                tile = &g_map_tile_data[i];
                state->destination_entry_height
                    = tile->depth_half_height.half_height * ((tile->slope_type >> state->source_side_shift) & 3);
                state->destination_opposite_height
                    = tile->depth_half_height.half_height * ((tile->slope_type >> state->destination_side_shift) & 3);
                state->destination_side_height_delta
                    = state->destination_entry_height - state->destination_opposite_height;
                if (config->jump_or_1f < tile->depth_half_height.half_height) {
                    state->destination_climb = 1;
                }
                if (tile->depth_half_height.half_height >= 3 && (frontier[i] & FRONTIER_FLAG_LOW_CORNER)) {
                    state->destination_climb = 1;
                }
                if (state->destination_side_height_delta >= 0) {
                    state->destination_climb = 0;
                }
            }
            state->source_climb = 0;
            if (!state->source_unit_record_index) {
                if (config->jump_or_1f < state->current_tile->depth_half_height.half_height) {
                    state->source_climb = 1;
                }
                if (state->current_tile->depth_half_height.half_height >= 3
                    && (frontier[state->tile_index] & FRONTIER_FLAG_LOW_CORNER)) {
                    state->source_climb = 1;
                }
            }
            level = state->work_level * 32;
            state->source_unit_record_index = state->destination_unit_record_index;
            flag = state->destination_unit_record_index << 4;
            if (step == state->path_length - 2 && g_move_path_height_offsets.source_present) {
                flag = 0x10;
            }
            source_bit = state->source_climb << 2;
            destination_bit = state->destination_climb << 3;
            /* Pack the complete path byte; individual field stores add truncation and change scheduling. */
            *(u8*)&g_move_path.data.steps[step]
                = (state->step_distance - 1) | (source_bit | (destination_bit | (flag | (dir | level))));
            state->tile_x = state->work_x;
            state->tile_y = state->work_y;
            state->tile_level = state->work_level;
            break;
        }
    }
    g_move_path.count = state->path_length - 1;
    if (config->unit_id >= BATTLE_UNIT_SLOT_COUNT) {
        g_move_effective_flags = 0;
    } else {
        g_move_effective_flags = move_get_effective_flags(&g_battle_unit_stats[config->unit_id]);
    }
}

/*
 * Prepare source-side heights and ceiling for a movement candidate.
 *
 * Return 1 when the source geometry rejects the candidate, otherwise 0.
 * Extended unit records and ordinary terrain use different height checks;
 * only the terrain path adds water depth after finding the ceiling.
 */
s32 move_init_source_geometry(s32 direction) {
    move_spread_state_t* state = g_move_scratch_pad_ptr;
    u8* frontier = g_move_frontier_flags_ptr;
    move_record_t* records = g_move_records_ptr;
    s32 difference;

    if (state->outer_count >= 5) {
        /* Typed pointer addition reverses the target's address operands. */
        move_record_t* record = (move_record_t*)(state->source_unit_record_index * sizeof(*records) + (u32)records);
        state->source_exit_height = record->standing_height + g_move_height_offsets[6 - direction / 4];
        state->work_height = state->source_base_height_times_two
            + state->source_half_height * ((state->source_slope >> state->source_side_shift) & 3);
        if (state->source_exit_height < state->work_height)
            return 1;
        state->source_ceiling_height = move_calculate_tile_ceiling(state->tile_x, state->tile_y,
            state->source_side_shift, (records[state->source_unit_record_index].standing_height + 5));
        state->source_side_height_delta = 0;
    } else if (state->source_unit_record_flag != 0) {
        if (!(frontier[state->tile_index] & FRONTIER_FLAG_STAND_ON_UNIT))
            return 1;
        state->source_exit_height = records[state->source_unit_record_index].top_height;
        state->source_opposite_height = state->source_exit_height;
        state->source_ceiling_height = move_calculate_tile_ceiling(
            state->tile_x, state->tile_y, state->source_side_shift, (state->source_exit_height - 1));
        state->source_side_height_delta = 0;
    } else {
        state->source_exit_height = state->source_base_height_times_two
            + state->source_half_height * ((state->source_slope >> state->source_side_shift) & 3);
        state->source_opposite_height = state->source_base_height_times_two
            + state->source_half_height * ((state->source_slope >> state->destination_side_shift) & 3);
        state->source_ceiling_height = move_calculate_tile_ceiling(
            state->tile_x, state->tile_y, state->source_side_shift, state->source_exit_height);
        difference = state->source_opposite_height - state->source_exit_height;
        state->source_side_height_delta = difference;
        if (difference > 0 && (frontier[state->tile_index] & FRONTIER_FLAG_STEEP))
            return 1;
        state->source_exit_height += state->current_tile->depth_half_height.depth * 2;
    }
    return 0;
}

/* Prepare destination geometry for a movement candidate.
 *
 * Tile candidates use the selected map layer. unit-record candidates use
 * compact standing/top heights and an auxiliary targeting panel.
 */
s32 move_init_destination_geometry(s32 candidate) {
    move_pathfind_scratch_t* config = g_move_config_ptr;
    move_spread_state_t* state = g_move_scratch_pad_ptr;
    u8* tile_flags = g_move_frontier_flags_ptr;
    move_record_t* records = g_move_records_ptr;

    state->destination_unit_record_flag = candidate >= 2;
    state->work_level = candidate & 1;
    if (state->inner_count >= 5)
        state->work_level = config->high_elevation;
    state->destination_index
        = state->work_level * MAP_TILES_PER_LAYER + state->work_y * config->map_max_x + state->work_x;
    state->destination_tile = &g_map_tile_data[state->destination_index];
    state->destination_panel = &g_target_panels[state->destination_index];
    state->destination_base_height_times_two = state->destination_tile->height * 2;
    state->destination_half_height = state->destination_tile->depth_half_height.half_height;
    state->destination_slope = state->destination_tile->slope_type;
    if (state->inner_count >= 5 || state->destination_unit_record_flag != 0) {
        state->destination_unit_record_index = state->destination_panel->unit_record_index;
        if (state->inner_count >= 5) {
            state->destination_unit_record_flag = 1;
            if (!(tile_flags[state->destination_index] & FRONTIER_FLAG_MOUNTABLE_UNIT))
                return 1;
            state->destination_entry_height
                = records[state->destination_unit_record_index].standing_height + g_move_height_offsets[candidate];
            state->work_height = state->destination_base_height_times_two
                + state->destination_half_height * ((state->destination_slope >> state->destination_side_shift) & 3);
            if (state->destination_entry_height < state->work_height)
                return 2;
        } else {
            if (!(tile_flags[state->destination_index] & FRONTIER_FLAG_STAND_ON_UNIT))
                return 3;
            state->destination_entry_height = records[state->destination_unit_record_index].top_height;
        }
        state->destination_side_height_delta = 0;
        state->destination_opposite_height = state->destination_entry_height;
        state->destination_ceiling_height = move_calculate_tile_ceiling(
            state->work_x, state->work_y, state->destination_side_shift, (state->destination_entry_height - 1));
        state->destination_panel_index = state->destination_unit_record_index + MAP_TILE_COUNT;
        state->effective_destination_panel = &g_target_panels[state->destination_panel_index];
    } else {
        if (!(tile_flags[state->destination_index] & FRONTIER_FLAG_ENTERABLE))
            return 4;
        state->destination_entry_height = state->destination_base_height_times_two
            + state->destination_half_height * ((state->destination_slope >> state->destination_side_shift) & 3);
        state->destination_opposite_height = state->destination_base_height_times_two
            + state->destination_half_height * ((state->destination_slope >> state->source_side_shift) & 3);
        state->destination_side_height_delta = state->destination_entry_height - state->destination_opposite_height;
        state->destination_ceiling_height = move_calculate_tile_ceiling(
            state->work_x, state->work_y, state->destination_side_shift, state->destination_entry_height);
        if (state->destination_side_height_delta >= 0 && (tile_flags[state->destination_index] & FRONTIER_FLAG_STEEP))
            return 5;
        state->clearance_height = state->source_exit_height;
        if (state->destination_tile->depth_half_height.depth
            && (config->movement_set_3 & (BATTLE_MOVEMENT_SET_3_MOVE_IN_WATER | BATTLE_MOVEMENT_SET_3_WALK_ON_WATER))) {
            state->destination_entry_height += state->destination_tile->depth_half_height.depth * 2;
            if (config->movement_set_3 & BATTLE_MOVEMENT_SET_3_WALK_ON_WATER)
                state->destination_entry_height -= 2;
        }
        state->destination_panel_index = state->destination_index;
        state->effective_destination_panel = &g_target_panels[state->destination_panel_index];
    }
    return 0;
}

/* Check a movement candidate and update its mode-dependent budget state.
 *
 * AI mode returns 11 after an accepted update; panel byte 4 breaks equal
 * budgets by the smaller immediate height difference. Non-AI mode returns 0
 * after geometry checks and sets budget_matches only when the budget matches.
 */
s32 move_update_candidate(s32 extra_span) {
    move_spread_state_t* state = g_move_scratch_pad_ptr;
    move_pathfind_scratch_t* config = g_move_config_ptr;
    u8* terrain_costs = g_move_terrain_costs_ptr;
    s32 step_cost;

    if (state->source_ceiling_height < state->destination_entry_height + config->unit_size)
        return 1;
    if (state->destination_ceiling_height < state->clearance_height + config->unit_size)
        return 2;
    if (state->source_ceiling_height < state->clearance_height + config->unit_size)
        return 3;
    if (state->destination_entry_height < state->source_exit_height) {
        if (extra_span != 0)
            return 4;
        state->height_delta = state->source_exit_height - state->destination_entry_height;
    } else {
        state->height_delta = state->destination_entry_height - state->source_exit_height;
    }
    if (state->height_delta > config->jump_times_two)
        return 7;

    if (config->ai_propagation_mode != 0) {
        state->candidate_remaining_range
            = state->current_panel->remaining_range - extra_span - terrain_costs[state->current_tile->surface.type];
        if (state->candidate_remaining_range <= 0)
            return 8;
        if ((u8)state->candidate_remaining_range < state->destination_panel->remaining_range)
            return 9;
        if ((u8)state->candidate_remaining_range == state->destination_panel->remaining_range
            && state->height_delta >= state->destination_panel->max_height_delta)
            return 10;
        state->destination_panel->remaining_range = (u8)state->candidate_remaining_range;
        state->destination_panel->max_height_delta = state->height_delta;
        if (state->candidate_remaining_range >= 2) {
            g_move_frontier_flags_ptr[state->destination_index] |= FRONTIER_FLAG_QUEUED;
            if ((u8)state->candidate_remaining_range > state->frontier_max_remaining_range)
                state->frontier_max_remaining_range = (u8)state->candidate_remaining_range;
        }
        return 11;
    }
    step_cost = extra_span + 1;
    if (state->outer_count >= 5 || state->source_unit_record_flag != 0) {
        if (state->candidate_remaining_range == state->effective_destination_panel->remaining_range - step_cost)
            state->budget_matches = 1;
    } else {
        if (state->candidate_remaining_range
            == state->effective_destination_panel->remaining_range
                - g_move_terrain_costs_ptr[state->current_tile->surface.type] - extra_span)
            state->budget_matches = 1;
    }
    return 0;
}

/* Spread movement to ordinary neighboring candidates.
 *
 * Direction callbacks prepare each candidate; accepted movement updates use
 * zero extra span. Outside AI mode, candidate_saved enables the common finalizer.
 */
void move_spread_to_neighbors(void) {
    move_pathfind_scratch_t* config = g_move_config_ptr;
    move_spread_state_t* state = g_move_scratch_pad_ptr;
    s32 direction;
    s32 candidate;
    s32 callback;

    move_init_spread_scratch();
    for (direction = 0; direction < state->outer_count; direction++) {
        callback = direction % 4;
        g_move_spread_preset_table[callback]();
        move_set_spreading_tile_x_and_y_coordinates();
        if (move_check_spreading_tile_coordinates() == 0 && move_init_source_geometry(direction) == 0) {
            if (state->inner_count > 4 && (config->x != state->work_x || config->y != state->work_y))
                continue;
            for (candidate = 0; candidate < state->inner_count; candidate++) {
                if (move_init_destination_geometry(candidate) == 0 && move_update_candidate(0) == 0
                    && state->budget_matches != 0)
                    move_save_selected_candidate(direction, candidate, 0);
            }
        }
    }
    if (config->ai_propagation_mode == 0 && state->candidate_saved != 0)
        move_apply_selected_candidate(state);
}

/*
 * Apply the saved movement candidate to the current spreading state.
 *
 * Copy the saved coordinates and residual, then restore half-height offsets
 * when their saved presence bits are set. The destination update uses byte
 * masks because its equivalent bitfield assignments change register allocation.
 */
void move_apply_selected_candidate(move_spread_state_t* state) {
    state->candidate_remaining_range = state->saved_remaining_range;
    state->tile_x = state->saved_x;
    state->tile_y = state->saved_y;
    state->tile_level = state->saved_level;
    if (state->saved_height_offset_flags & MOVE_SAVED_SOURCE_HEIGHT) {
        move_height_offsets_t* offsets = &g_move_path_height_offsets;
        move_height_offsets_t packed = *offsets;
        packed.source_height_offset = state->saved_source_height_offset;
        packed.source_present = 1;
        *offsets = packed;
    }
    if (state->saved_height_offset_flags & MOVE_SAVED_DESTINATION_HEIGHT) {
        u8* offsets = (u8*)&g_move_path_height_offsets;
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

/* Horizontal jumps: from the current tile, in each direction, try landing 2 to
 * jump_half + 1 tiles away, provided every tile in between can be jumped over
 * at the landing height (move_check_horizontal_jump). The best
 * candidate is saved and, outside AI propagation, applied to the spread state. */
void move_calculate_spread(void) {
    move_pathfind_scratch_t* config = g_move_config_ptr;
    move_spread_state_t* state = g_move_scratch_pad_ptr;
    s32 outer_index;
    s32 inner_index;
    s32 pass_index;
    s32 path_index;
    s32 callback_index;

    move_init_spread_scratch();
    for (outer_index = 0; outer_index < state->outer_count; outer_index++) {
        callback_index = outer_index % 4;
        g_move_spread_preset_table[callback_index]();
        if (move_init_source_geometry(outer_index) == 0) {
            pass_index = 1;
            move_set_spreading_tile_x_and_y_coordinates();
            if (config->jump_half != 0) {
                do {
                    if (move_check_spreading_tile_coordinates() == 0) {
                        state->work_x = state->tile_x + state->x_step * (pass_index + 1);
                        state->work_y = state->tile_y + state->y_step * (pass_index + 1);
                        if (move_check_spreading_tile_coordinates() == 0) {
                            for (inner_index = 0; inner_index < state->inner_count; inner_index++) {
                                if (move_init_destination_geometry(inner_index) == 0) {
                                    state->placement_failed = 0;
                                    for (path_index = 1; path_index <= pass_index; path_index++) {
                                        if (move_check_horizontal_jump(
                                                (state->work_y - state->y_step * path_index) * config->map_max_x
                                                    + (state->work_x - state->x_step * path_index),
                                                state->source_side_shift, state->destination_side_shift,
                                                state->destination_entry_height)
                                            != 0) {
                                            state->placement_failed = 1;
                                            break;
                                        }
                                    }
                                    if (state->placement_failed == 0 && move_update_candidate(pass_index) == 0
                                        && state->budget_matches != 0) {
                                        move_save_selected_candidate(outer_index, inner_index, pass_index);
                                    }
                                }
                            }
                        } else {
                            break;
                        }
                    } else {
                        break;
                    }
                    pass_index++;
                } while (pass_index <= config->jump_half);
            }
        }
    }
    if (config->ai_propagation_mode == 0 && state->candidate_saved != 0) {
        move_apply_selected_candidate(state);
    }
}

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

/* move_calculate_pathing with the per-call search budget enabled, so a
 * long search can suspend and resume through `suspended`. */
void move_calculate_pathing_time_sliced(s32 flags, s32 jump, s32 x, s32 y, s32 level, s32 target_x, s32 target_y,
    s32 target_level, s32 initialize, s32* suspended) {
    move_calculate_pathing(flags, jump, x, y, level, target_x, target_y, target_level, initialize, suspended, 1);
}

/* Falls off the end: move_calculate_pathing leaves the path block pointer in $v0 and
 * the caller consumes it, so the result type belongs on this definition. */
battle_walk_path_t* move_calculate_walkto_pathing(
    s32 flags, s32 jump, s32 x, s32 y, s32 level, s32 target_x, s32 target_y, s32 target_level) {
    s32 suspended;
    move_calculate_pathing(flags, jump, x, y, level, target_x, target_y, target_level, 1, &suspended, 0);
}

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

/* Propagate the acting unit's movement budget outward from a target tile.
 *
 * Each pass snapshots the frontier before visiting it. A time-budget pause
 * saves scratchpad state and sets *suspended to 1 while returning 0.
 * Return -1 for an absent unit on initialization, or the reachable count
 * on completion. The caller must check suspension before using that count.
 */
s32 ai_propagate_target_movement(
    s32 unit_id, s32 target_x, s32 target_y, s32 target_elevation, s32 initialize, s32* suspended, s32 check_budget) {
    u8 frontier_snapshot[MAP_TILE_COUNT];
    move_pathfind_scratch_t* config = g_move_config_ptr;
    move_spread_state_t* state = g_move_scratch_pad_ptr;
    move_record_t* records = g_move_records_ptr;
    u8* frontier = g_move_frontier_flags_ptr;
    s32 completed_pass = 0;
    s32 pass;
    s32 index;
    target_panel_t* panel;

    *suspended = 0;
    if (initialize) {
        if (g_battle_unit_stats[unit_id].entd_slot == BATTLE_ENTD_SLOT_NONE)
            return -1;
        move_store_unit_movement_to_scratchpad(unit_id);
        config->move = MOVE_PATHFIND_RANGE;
        for (pass = MOVE_RECORD_COUNT - 1; pass >= 0; pass--)
            records[pass].unit_id_flags = MOVE_RECORD_NONE;
        move_set_tile_flags_for_pathfinding(MOVE_TILE_FLAGS_MODE_START_SPREAD);
        index = (config->high_elevation * MAP_TILES_PER_LAYER) + config->y * config->map_max_x + config->x;
        panel = &g_target_panels[index];
        panel->ride_remaining_range = 0;
        panel->remaining_range = 0;
        frontier[index] &= ~FRONTIER_FLAG_QUEUED;
        state->outer_count = 4;
        state->inner_count = 4;
        panel->remaining_range = 0;
        panel->max_height_delta = 0xff;
        index = (target_elevation * MAP_TILES_PER_LAYER) + target_y * config->map_max_x + target_x;
        panel = &g_target_panels[index];
        panel->remaining_range = MOVE_PATHFIND_RANGE + 1;
        frontier[index] |= FRONTIER_FLAG_QUEUED;
        panel->max_height_delta = 0;
        g_ai_propagation_resume_pass = 0;
        config->ai_propagation_mode = 1;
    } else {
        /* A direct scratchpad address avoids an extra linker-address addiu. */
        mem_copy_byte_data_source_first(g_ai_unit_snapshot_storage, getScratchAddr(0), PSX_SCRATCHPAD_BYTE_COUNT);
    }
    if (config->move_type == MOVE_CLASS_TELEPORT) {
        /* Teleport and Teleport 2 bypass propagation, not destination filtering. */
        u8 remaining_range = 1;
        target_panel_t* fill_panels = g_target_panels;
        for (pass = MAP_TILE_COUNT - 1; pass >= 0; pass--)
            fill_panels[pass].remaining_range = remaining_range;
    } else {
        for (pass = g_ai_propagation_resume_pass; pass < config->move; pass++) {
            if (check_budget && ai_is_pathfinding_budget_exhausted() && completed_pass) {
                g_ai_propagation_resume_pass = pass;
                *suspended = 1;
                mem_copy_byte_data_source_first(
                    getScratchAddr(0), g_ai_unit_snapshot_storage, PSX_SCRATCHPAD_BYTE_COUNT);
                return 0;
            }
            if (!state->frontier_max_remaining_range)
                break;
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
                            state->source_unit_record_flag = 0;
                            move_spread_to_neighbors();
                            if (!config->fly_or_teleport) {
                                state->source_unit_record_flag = 0;
                                move_calculate_spread();
                            }
                        }
                    }
                }
            }
            completed_pass = 1;
        }
    }
    move_set_reachable_tile_flags();
    g_move_reachable_tiles_valid = 1;
    g_move_reachable_unit_id = config->unit_id;
    return state->reachable_count;
}
