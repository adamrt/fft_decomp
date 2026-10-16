/* Movement range spreading and pathfinding, teleport chances, path encoding, and targeting panel ranges for abilities
 * and weapons. */

#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_ai.h"
#include "fft/battle_effect.h"
#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "fft/battle_move.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "fft/data.h"
#include "fft/main.h"
#include "fft/main_item.h"
#include "fft/main_unit.h"
#include "fft/map.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/unit.h"
#include "psx/libetc.h"
#include "psx/types.h"

static void battle_move_apply_selected_candidate(move_spread_state_t* state);
static battle_walk_path_t* battle_move_calculate_pathing(s32 flags, s32 jump, s32 x, s32 y, u32 level, s32 target_x,
    s32 target_y, u32 target_level, s32 initialize, s32* suspended, s32 check_budget);
static void battle_move_calculate_spread(void);
static s32 battle_move_calculate_teleport_chances(void);
static u8 battle_move_calculate_tile_ceiling(s32 x, s32 y, s32 exit_slope_shift, u8 exit_height);
static void battle_move_check_and_spread_one_tile_around(void);
static void battle_move_check_occupied_tile_standing_height(s32 record_index, s32 extra_span);
static void battle_move_encode_path_steps(void);
static void battle_move_save_selected_candidate(s32 direction, s32 candidate, s32 extra_span);
static void battle_move_set_reachable_tile_flags(void);
static void battle_move_set_tile_flags_for_pathfinding(s32 mode);
static void battle_move_spread_horizontal_jump(void);
static void battle_move_spread_to_neighbors(void);
static void battle_move_load_tile_height_for_spread(void);
static void battle_target_apply_unit_team_eligibility(
    s32 raw_unit_id, u8 allow_allies, u8 allow_enemies, u8 aoe_is_0xff);
static void battle_target_apply_vertical_fixed(s32 x, s32 y);
static void battle_target_build_directional_attack_panels(const action_command_t* source, u8 mode);
static void battle_target_calculate_aoe_vertical_tolerance(s32 x, s32 y, s32 lo, s32 hi);
static void battle_target_calculate_linear_attack_tiles(s32 dir, s32 x, s32 y);
static void battle_target_calculate_strike_lunge_range(const unit_stats_t* unit, u8 flags);
static void battle_target_clear_range_on_untargetable_tiles(void);
static s32 battle_target_clear_selection_state_of_all_panels(void);
static s32 battle_target_keep_only_target_tile(const action_command_t* action);
static s32 battle_target_run_calculator(const action_command_t* source);
static s32 battle_target_set_all_panels_targeted_if_targetable(void);
static s32 battle_target_set_green_panels_for_action(const action_command_t* action);
static void battle_target_set_state_for_all_unit_panels(const unit_stats_t* unit, s32 flags);
static s32 battle_target_spread_panel_to_neighbors(s32 y, s32 x);
static s32 battle_target_validate_height_overlap(
    s32 attacker_id, s32 target_x, s32 target_y, s32 target_elevation, s32 target_id);
static s32 battle_target_validate_lunging_target(s32 unit_id, s32 x, s32 y, s32 elevation, s32 target_id);

static s32 battle_move_get_weather_terrain_cost(void) {
    s32 weather = battle_map_get_effective_weather();

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
static void battle_move_store_unit_movement_to_scratchpad(s32 unit_id) {
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

    config = g_battle_move_config_ptr;
    record = g_battle_move_records_ptr;
    battle_unit_calculate_height_data(&height_data, unit_id);
    unit = &g_battle_unit_stats[unit_id];
    for (i = 0; i < 3; i++) {
        movement[i] = unit->ability_flags.movement[i];
    }
    config->unit_id = unit_id;
    config->x = unit->x;
    config->y = unit->position.y;
    config->high_elevation = unit->position.higher_elevation;
    config->map_max_x = g_battle_map_max_x;
    config->map_max_y = g_battle_map_max_y;
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
    weather = battle_move_get_weather_terrain_cost();
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
        g_battle_move_terrain_costs_ptr[i] = byte;
    }
    value = unit->jump;
    if (value >= 8) {
        value = 7;
    }
    config->jump_half = value >> 1;
    if (config->move_type == MOVE_CLASS_FLY || (movement[0] & BATTLE_MOVEMENT_SET_1_IGNORE_HEIGHT)) {
        value = 0x1F;
    }
    config->jump_times_two = value * 2;
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
        tile = &g_battle_map_tile_data[battle_map_calculate_location(other)];
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
s32 battle_move_set_reachable_tiles(s32 unit_id, s32 unused_map_x, s32 unused_map_y, s32 unused_map_z) {
    move_pathfind_scratch_t* config = g_battle_move_config_ptr;
    move_spread_state_t* state = g_battle_move_scratch_pad_ptr;
    u8* frontier = g_battle_move_frontier_flags_ptr;
    s32 pass;

    if (g_battle_unit_stats[unit_id].entd_slot == BATTLE_ENTD_SLOT_NONE) {
        return -1;
    }
    battle_move_store_unit_movement_to_scratchpad(unit_id);
    battle_move_set_tile_flags_for_pathfinding(MOVE_TILE_FLAGS_MODE_START_SPREAD);
    if (config->move_type == MOVE_CLASS_TELEPORT) {
        u8 remaining_range = 1;
        target_panel_t* fill_panels = g_battle_target_panels;
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
                        state->current_tile = &g_battle_map_tile_data[state->tile_index];
                        state->current_panel = &g_battle_target_panels[state->tile_index];
                        if (frontier[state->tile_index] & FRONTIER_FLAG_QUEUED) {
                            frontier[state->tile_index] &= ~FRONTIER_FLAG_QUEUED;
                            battle_move_check_and_spread_one_tile_around();
                            if (!config->fly_or_teleport) {
                                battle_move_spread_horizontal_jump();
                            }
                            state->outer_count = 8;
                        }
                    }
                }
            }
        }
    }
    battle_move_set_reachable_tile_flags();
    g_battle_move_reachable_tiles_valid = 1;
    g_battle_move_reachable_unit_id = config->unit_id;
    return state->reachable_count;
}

/* Clears the enterable flag on the tile of every dead or jumping unit. */
static void battle_move_clear_enterable_flags_under_dead_or_jumping_units(void) {
    s32 unit_id;

    for (unit_id = 0; unit_id < BATTLE_UNIT_SLOT_COUNT; unit_id++) {
        unit_stats_t* unit = &g_battle_unit_stats[unit_id];

        if ((unit->entd_slot != BATTLE_ENTD_SLOT_NONE)
            && ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DEAD)]
                    & (STATUS_BYTE_MASK(STATUS_ID_DEAD) | STATUS_BYTE_MASK(STATUS_ID_JUMP)))
                != 0)) {
            g_battle_move_frontier_flags_ptr[battle_map_calculate_location(unit)] &= ~FRONTIER_FLAG_ENTERABLE;
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
static void battle_move_set_tile_flags_for_pathfinding(s32 mode) {
    move_pathfind_scratch_t* config = g_battle_move_config_ptr;
    move_spread_state_t* state = g_battle_move_scratch_pad_ptr;
    u8* frontier = g_battle_move_frontier_flags_ptr;
    move_record_t* records = g_battle_move_records_ptr;
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
        panel = &g_battle_target_panels[i];
        panel->mark = 0;
        panel->unit_record_index = 0;
        if (mode & 1) {
            panel->remaining_range = 0;
            panel->ride_remaining_range = 0;
            panel->max_height_delta = 0xff;
        }
        if (i < MAP_TILE_COUNT) {
            tile = &g_battle_map_tile_data[i];
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
                g_battle_target_panels[index].unit_record_index = i;
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
        panel = &g_battle_target_panels[index];
        if (frontier[index] & FRONTIER_FLAG_MOUNTABLE_UNIT) {
            state->outer_count = 0x24;
            aux = g_battle_target_panels + MAP_TILE_COUNT + panel->unit_record_index;
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
static void battle_move_set_reachable_tile_flags(void) {
    move_pathfind_scratch_t* config = g_battle_move_config_ptr;
    move_spread_state_t* spread = g_battle_move_scratch_pad_ptr;
    u8* flags = g_battle_move_frontier_flags_ptr;
    s32 self = (config->high_elevation << 8) + config->y * config->map_max_x + config->x;
    s32 i;
    map_tile_t* tile;
    target_panel_t* panel;
    s32 half_height;
    s32 surface;
    map_tile_flags_t tile_flags;
    map_tile_depth_t water_depth;
    s32 frontier;

    battle_move_clear_enterable_flags_under_dead_or_jumping_units();
    for (i = 0; i < MAP_TILE_COUNT; i++) {
        tile = &g_battle_map_tile_data[i];
        tile->ceiling_depth_and_marks.move_destination = 0;
        panel = &g_battle_target_panels[i];
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
void battle_move_spread_to_east_preset(void) {
    move_spread_state_t* spread = g_battle_move_scratch_pad_ptr;
    spread->x_step = 1;
    spread->y_step = 0;
    spread->source_side_shift = 0;
    spread->destination_side_shift = 2;
}

/* Step +y: the slope shifts select the source's exit side and the
 * destination's entry side. */
void battle_move_spread_to_north_preset(void) {
    move_spread_state_t* spread = g_battle_move_scratch_pad_ptr;
    spread->y_step = 1;
    spread->source_side_shift = 6;
    spread->x_step = 0;
    spread->destination_side_shift = 4;
}

/* Step -x: the slope shifts select the source's exit side and the
 * destination's entry side. */
void battle_move_spread_to_west_preset(void) {
    move_spread_state_t* spread = g_battle_move_scratch_pad_ptr;
    spread->x_step = -1;
    spread->y_step = 0;
    spread->source_side_shift = 2;
    spread->destination_side_shift = 0;
}

/* Step -y: the slope shifts select the source's exit side and the
 * destination's entry side. */
void battle_move_spread_to_south_preset(void) {
    move_spread_state_t* spread = g_battle_move_scratch_pad_ptr;
    spread->y_step = -1;
    spread->source_side_shift = 4;
    spread->x_step = 0;
    spread->destination_side_shift = 6;
}

/* Reset the best-candidate fields of the spread state and load the current
 * tile's heights. */
static void battle_move_init_spread_scratch(void) {
    move_spread_state_t* spread = g_battle_move_scratch_pad_ptr;
    spread->best_height_delta = 0xFF;
    spread->best_fly_height_delta = 0xFF;
    spread->budget_matches = 0;
    spread->candidate_saved = 0;
    spread->best_extra_span = 0x7F;
    battle_move_load_tile_height_for_spread();
}

/* Load the current tile's base height (doubled, in half-height units), half
 * height and slope into the spread state. */
static void battle_move_load_tile_height_for_spread(void) {
    move_spread_state_t* state = g_battle_move_scratch_pad_ptr;
    state->source_base_height_times_two = state->current_tile->height << 1;
    state->source_half_height = state->current_tile->depth_half_height.half_height;
    state->source_slope = state->current_tile->slope_type;
}

/* Work tile = current tile + one step of the preset direction. */
static void battle_move_set_spreading_tile_x_and_y_coordinates(void) {
    move_spread_state_t* spread = g_battle_move_scratch_pad_ptr;
    spread->work_x = spread->tile_x + spread->x_step;
    spread->work_y = spread->tile_y + spread->y_step;
}

/*
 * Check whether the spreading coordinates lie outside the map.
 *
 * Return 1 for an invalid coordinate, or 0 for an in-bounds tile.
 */
static s32 battle_move_check_spreading_tile_coordinates(void) {
    move_spread_state_t* state = g_battle_move_scratch_pad_ptr;
    move_pathfind_scratch_t* config = g_battle_move_config_ptr;
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
static s32 battle_move_init_source_panel(s32 direction) {
    move_pathfind_scratch_t* config = g_battle_move_config_ptr;
    u8* frontier = g_battle_move_frontier_flags_ptr;
    move_spread_state_t* state = g_battle_move_scratch_pad_ptr;
    move_record_t* records = g_battle_move_records_ptr;

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
        state->source_ceiling_height = battle_move_calculate_tile_ceiling(
            state->tile_x, state->tile_y, state->source_side_shift, (state->source_exit_height - 1));
        panel_index = state->source_unit_record_index + MAP_TILE_COUNT;
        state->source_panel_index = panel_index;
        state->source_panel = &g_battle_target_panels[panel_index];
    } else {
        state->source_exit_height = state->source_base_height_times_two
            + state->source_half_height * ((state->source_slope >> state->source_side_shift) & 3);
        state->source_opposite_height = state->source_base_height_times_two
            + state->source_half_height * ((state->source_slope >> state->destination_side_shift) & 3);
        state->source_side_height_delta = state->source_exit_height - state->source_opposite_height;
        state->source_ceiling_height = battle_move_calculate_tile_ceiling(
            state->tile_x, state->tile_y, state->source_side_shift, state->source_exit_height);
        if (state->source_side_height_delta >= 0 && (frontier[state->tile_index] & FRONTIER_FLAG_STEEP))
            return 3;
        if (state->current_tile->depth_half_height.depth
            && (config->movement_set_3 & (BATTLE_MOVEMENT_SET_3_MOVE_IN_WATER | BATTLE_MOVEMENT_SET_3_WALK_ON_WATER))) {
            state->source_exit_height += state->current_tile->depth_half_height.depth * 2;
            if (config->movement_set_3 & BATTLE_MOVEMENT_SET_3_WALK_ON_WATER)
                state->source_exit_height -= 2;
        }
        state->source_panel = &g_battle_target_panels[state->source_panel_index = state->tile_index];
    }
    return (state->source_panel->remaining_range < 2) * 4;
}

/*
 * Check a destination path and propagate an improved movement budget.
 *
 * Return 0 for an accepted path, otherwise a numeric rejection reason.
 * The mount helper can update separate state before a later rejection.
 */
static s32 battle_move_propagate_destination(s32 candidate, s32 extra_span) {
    move_spread_state_t* state = g_battle_move_scratch_pad_ptr;
    move_pathfind_scratch_t* config = g_battle_move_config_ptr;
    u8* flags = g_battle_move_frontier_flags_ptr;
    move_record_t* records = g_battle_move_records_ptr;
    u8* terrain_costs = g_battle_move_terrain_costs_ptr;
    u8* destination_flags;

    state->work_level = candidate & 1;
    state->destination_unit_record_flag = candidate / 2;
    state->destination_tile = &g_battle_map_tile_data[state->destination_index
        = state->work_level * MAP_TILES_PER_LAYER + state->work_y * config->map_max_x + state->work_x];
    state->destination_panel = &g_battle_target_panels[state->destination_index];
    state->destination_base_height_times_two = state->destination_tile->height * 2;
    state->destination_half_height = state->destination_tile->depth_half_height.half_height;
    state->destination_slope = state->destination_tile->slope_type;
    destination_flags = &flags[state->destination_index];
    if (state->destination_unit_record_flag != 0) {
        if (!(*destination_flags & (FRONTIER_FLAG_MOUNTABLE_UNIT | FRONTIER_FLAG_STAND_ON_UNIT)))
            return 1; /* no unit there to mount or stand on */
        state->destination_unit_record_index = state->destination_panel->unit_record_index;
        state->destination_entry_height = records[state->destination_unit_record_index].top_height;
        state->destination_ceiling_height = battle_move_calculate_tile_ceiling(
            state->work_x, state->work_y, state->destination_side_shift, (state->destination_entry_height - 1));
        if (*destination_flags & FRONTIER_FLAG_MOUNTABLE_UNIT)
            battle_move_check_occupied_tile_standing_height(state->destination_unit_record_index, extra_span);
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
        state->destination_ceiling_height = battle_move_calculate_tile_ceiling(
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
            < g_battle_target_panels[state->destination_unit_record_index + MAP_TILE_COUNT].remaining_range)
            return 12; /* the unit top is already reached with more range left */
        if ((u8)state->candidate_remaining_range
                == g_battle_target_panels[state->destination_unit_record_index + MAP_TILE_COUNT].remaining_range
            && state->height_delta
                >= g_battle_target_panels[state->destination_unit_record_index + MAP_TILE_COUNT].max_height_delta)
            return 13; /* same range left, and no smaller height difference */
        g_battle_target_panels[state->destination_unit_record_index + MAP_TILE_COUNT].remaining_range
            = (u8)state->candidate_remaining_range;
        g_battle_target_panels[state->destination_unit_record_index + MAP_TILE_COUNT].max_height_delta
            = state->height_delta;
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
        g_battle_move_frontier_flags_ptr[state->destination_index] |= FRONTIER_FLAG_QUEUED;
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
static void battle_move_check_and_spread_one_tile_around(void) {
    move_spread_state_t* state = g_battle_move_scratch_pad_ptr;
    s32 i;
    s32 j;

    battle_move_load_tile_height_for_spread();
    for (i = 0; i < state->outer_count; i++) {
        state->source_unit_record_flag = i >= 4;
        g_battle_move_spread_preset_table[i % 4]();
        battle_move_set_spreading_tile_x_and_y_coordinates();
        if ((battle_move_check_spreading_tile_coordinates() == 0) && (battle_move_init_source_panel(i) == 0)) {
            for (j = 0; j < state->inner_count; j++) {
                battle_move_propagate_destination(j, 0);
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
static void battle_move_check_occupied_tile_standing_height(s32 record_index, s32 extra_span) {
    move_spread_state_t* state;
    move_pathfind_scratch_t* config;
    move_record_t* records;
    target_panel_t* panel;
    s32 step_cost;
    s32 remaining;
    u8 height = record_index;
    u8 value;
    u8 limit;
    u8 top;

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
static u8 battle_move_calculate_tile_ceiling(s32 x, s32 y, s32 exit_slope_shift, u8 exit_height) {
    s32 elevation;
    map_tile_t* tile;
    map_tile_depth_t depth;
    u8 ceiling = TILE_CEILING_NONE;
    u8 tile_ceiling;

    for (elevation = 0; elevation < MAP_TERRAIN_LAYER_COUNT; elevation++) {
        tile = &g_battle_map_tile_data[elevation * MAP_TILES_PER_LAYER + y * g_battle_move_config_ptr->map_max_x + x];
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
static s32 battle_move_check_horizontal_jump(s32 tile_index, u8 shift_a, u8 shift_b, u8 height) {
    move_spread_state_t* state;
    move_record_t* records;
    u8* frontier;
    map_tile_t* tile;
    s32 index;
    s32 layer;
    s32 low;
    u8 high;
    s32 lowest = 0x7f;
    s32 height_a;
    s32 height_b;
    s32 depth;

    frontier = g_battle_move_frontier_flags_ptr;
    state = g_battle_move_scratch_pad_ptr;
    records = g_battle_move_records_ptr;
    state->jump_unit_size = g_battle_move_config_ptr->unit_size;
    for (layer = 0; layer < 4; layer++) {
        index = (layer / 2) * MAP_TILES_PER_LAYER + tile_index;
        tile = &g_battle_map_tile_data[index];
        if (layer & 1) {
            if ((frontier[index] & (FRONTIER_FLAG_UNIT_ON_TILE | FRONTIER_FLAG_STAND_ON_UNIT))
                != FRONTIER_FLAG_UNIT_ON_TILE) {
                continue;
            }
            state->work_height = g_battle_target_panels[index].unit_record_index;
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
static void battle_move_spread_horizontal_jump(void) {
    move_pathfind_scratch_t* config = g_battle_move_config_ptr;
    move_spread_state_t* state = g_battle_move_scratch_pad_ptr;
    s32 outer_index;
    s32 inner_index;
    s32 pass_index;

    battle_move_load_tile_height_for_spread();
    for (outer_index = 0; outer_index < state->outer_count; outer_index++) {
        state->source_unit_record_flag = outer_index >= 4;
        g_battle_move_spread_preset_table[outer_index % 4]();
        if (battle_move_init_source_panel(outer_index) == 0) {
            battle_move_set_spreading_tile_x_and_y_coordinates();
            for (pass_index = 1; pass_index <= config->jump_half; pass_index++) {
                state->jump_remaining_range = state->source_panel->remaining_range - pass_index;
                if (state->jump_remaining_range < 2) {
                    break;
                }
                if (battle_move_check_spreading_tile_coordinates() != 0) {
                    break;
                }
                if (battle_move_check_horizontal_jump(state->work_y * config->map_max_x + state->work_x,
                        state->source_side_shift, state->destination_side_shift, state->source_exit_height)
                    != 0) {
                    break;
                }
                state->work_x = state->tile_x + state->x_step * (pass_index + 1);
                state->work_y = state->tile_y + state->y_step * (pass_index + 1);
                if (battle_move_check_spreading_tile_coordinates() != 0) {
                    break;
                }
                for (inner_index = 0; inner_index < state->inner_count; inner_index++) {
                    battle_move_propagate_destination(inner_index, pass_index);
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
 * the path buffer at g_battle_move_path.
 *
 * The flag byte at 0x8018f86d is written through a pointer loaded twice: the
 * target materialises its address in a register in both blocks. */
battle_walk_path_t* battle_move_build_path_to_tile(s32 unit_id, s32 x, s32 y, s32 elevation) {
    move_height_offsets_t no_offsets = { 0 };
    move_pathfind_scratch_t* config = g_battle_move_config_ptr;
    move_spread_state_t* state = g_battle_move_scratch_pad_ptr;
    move_record_t* records = g_battle_move_records_ptr;
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
    if (!g_battle_map_tile_data[index].ceiling_depth_and_marks.move_destination) {
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
    battle_move_set_tile_flags_for_pathfinding(MOVE_TILE_FLAGS_MODE_KEEP_RANGES);
    g_battle_move_path_height_offsets = no_offsets;
    g_battle_move_destination_unit_record = 0;
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
        state->source_unit_record_index = g_battle_target_panels[index].unit_record_index;
        g_battle_move_destination_unit_record
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
        move_flags = &g_battle_move_effective_flags;
        *move_flags = BATTLE_EFFECTIVE_MOVEMENT_TELEPORT;
        value = config->movement_3;
        if (value & BATTLE_MOVEMENT_SET_3_MOVE_IN_WATER) {
            *move_flags = BATTLE_EFFECTIVE_MOVEMENT_TELEPORT | BATTLE_EFFECTIVE_MOVEMENT_ON_WATER;
        } else if (value & BATTLE_MOVEMENT_SET_3_WALK_ON_WATER) {
            *move_flags = BATTLE_EFFECTIVE_MOVEMENT_TELEPORT | BATTLE_EFFECTIVE_MOVEMENT_IN_WATER;
        }
        if (value & BATTLE_MOVEMENT_SET_3_SILENT_WALK) {
            move_flags = &g_battle_move_effective_flags;
            *move_flags |= BATTLE_EFFECTIVE_MOVEMENT_SILENT_WALK;
        }
        if (battle_move_calculate_teleport_chances() != 0) {
            g_battle_move_path.count = MOVE_PATH_TELEPORT;
            g_battle_move_path.data.teleport.x = x;
            g_battle_move_path.data.teleport.y = y;
            g_battle_move_path.data.teleport.elevation = elevation;
        } else {
            g_battle_move_path.count = MOVE_PATH_TELEPORT_FAILED;
        }
        return &g_battle_move_path;
    }
    state->candidate_remaining_range = 0xff;
    state->path_length = 0;
    state->inner_count = 4;
    state->selected_source_side_shift = 0;
    while (state->candidate_remaining_range != 0) {
        state->previous_source_side_shift = state->selected_source_side_shift;
        pass = state->path_length + 1;
        state->tile_index = state->tile_level * MAP_TILES_PER_LAYER + state->tile_y * config->map_max_x + state->tile_x;
        state->current_tile = &g_battle_map_tile_data[state->tile_index];
        state->current_panel = &g_battle_target_panels[state->tile_index];
        state->path_length = pass;
        if (state->outer_count >= 5) {
            value = state->current_panel->ride_remaining_range;
            range = state->current_panel->remaining_range;
            if (range < value) {
                g_battle_target_panels[state->source_unit_record_index + MAP_TILE_COUNT].ride_remaining_range = pass;
            } else {
                value = range;
                state->source_unit_record_flag = 0;
                state->outer_count = 4;
                state->current_panel->mark = state->path_length;
            }
        } else if (state->source_unit_record_flag != 0) {
            value = g_battle_target_panels[state->source_unit_record_index + MAP_TILE_COUNT].remaining_range;
            g_battle_target_panels[state->source_unit_record_index + MAP_TILE_COUNT].mark = pass;
        } else {
            value = state->current_panel->remaining_range;
            state->current_panel->mark = pass;
        }
        state->candidate_remaining_range = value;
        if (state->tile_x == config->x && state->tile_y == config->y && state->tile_level == config->high_elevation) {
            break;
        }
        battle_move_spread_to_neighbors();
        previous = state->candidate_remaining_range;
        if (previous != value) {
            state->outer_count = 4;
            continue;
        }
        battle_move_calculate_spread();
        current = state->candidate_remaining_range;
        if (current != previous) {
            state->outer_count = 4;
            continue;
        }
        if (config->source_tile_occupied != 0) {
            state->inner_count = 7;
            battle_move_spread_to_neighbors();
            previous = state->candidate_remaining_range;
            if (previous != current) {
                continue;
            }
            battle_move_calculate_spread();
            if (state->candidate_remaining_range != previous) {
                continue;
            }
        }
        return 0;
    }
    battle_move_encode_path_steps();
    return &g_battle_move_path;
}

/* 1 when the teleport succeeds: always with Teleport 2, otherwise it fails
 * with a 10 in 100 chance per tile of distance beyond Move. */
static s32 battle_move_calculate_teleport_chances(void) {
    move_pathfind_scratch_t* ctx = g_battle_move_config_ptr;
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

/* Encode the traced path as move_step_t bytes in g_battle_move_path,
 * count holding the number of encoded steps. Starting from the unit's tile, step n goes to
 * the tile or unit record the back-trace marked with path_length - n; the byte
 * records the direction, the distance, the level, whether the step ends on a
 * unit and whether either end is a climb. */
static void battle_move_encode_path_steps(void) {
    move_pathfind_scratch_t* config = g_battle_move_config_ptr;
    move_spread_state_t* state = g_battle_move_scratch_pad_ptr;
    move_record_t* records = g_battle_move_records_ptr;
    u8* frontier = g_battle_move_frontier_flags_ptr;
    move_record_t* record;
    map_tile_t* tile;
    s32 step;
    s32 path_index;
    u8 level;
    s32 i;
    u8 target;
    s32 rem;
    u8 hit;
    u8 dir;
    s32 flag;
    u8 source_bit;
    u8 destination_bit;

    state->source_unit_record_index = 0;
    for (step = 0; step < state->path_length - 1; step++) {
        state->tile_index = state->tile_level * MAP_TILES_PER_LAYER + state->tile_y * config->map_max_x + state->tile_x;
        state->current_tile = &g_battle_map_tile_data[state->tile_index];
        state->current_panel = &g_battle_target_panels[state->tile_index];
        path_index = step + 1;
        target = state->path_length - path_index;
        for (i = 0; i < MOVE_PANEL_COUNT; i++) {
            if (i >= MAP_TILE_COUNT && g_battle_target_panels[i].ride_remaining_range == target) {
                hit = 1;
                record = &records[i - MAP_TILE_COUNT];
            } else if (g_battle_target_panels[i].mark == target) {
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
                tile = &g_battle_map_tile_data[i];
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
            if (step == state->path_length - 2 && g_battle_move_path_height_offsets.source_present) {
                flag = 0x10;
            }
            source_bit = state->source_climb << 2;
            destination_bit = state->destination_climb << 3;
            /* Pack the complete path byte; individual field stores add truncation and change scheduling. */
            *(u8*)&g_battle_move_path.data.steps[step]
                = (state->step_distance - 1) | (source_bit | (destination_bit | (flag | (dir | level))));
            state->tile_x = state->work_x;
            state->tile_y = state->work_y;
            state->tile_level = state->work_level;
            break;
        }
    }
    g_battle_move_path.count = state->path_length - 1;
    if (config->unit_id >= BATTLE_UNIT_SLOT_COUNT) {
        g_battle_move_effective_flags = 0;
    } else {
        g_battle_move_effective_flags = battle_move_get_effective_flags(&g_battle_unit_stats[config->unit_id]);
    }
}

/*
 * Prepare source-side heights and ceiling for a movement candidate.
 *
 * Return 1 when the source geometry rejects the candidate, otherwise 0.
 * Extended unit records and ordinary terrain use different height checks;
 * only the terrain path adds water depth after finding the ceiling.
 */
static s32 battle_move_init_source_geometry(s32 direction) {
    move_spread_state_t* state = g_battle_move_scratch_pad_ptr;
    u8* frontier = g_battle_move_frontier_flags_ptr;
    move_record_t* records = g_battle_move_records_ptr;
    s32 difference;

    if (state->outer_count >= 5) {
        /* Typed pointer addition reverses the target's address operands. */
        move_record_t* record = (move_record_t*)(state->source_unit_record_index * sizeof(*records) + (u32)records);
        state->source_exit_height = record->standing_height + g_battle_move_height_offsets[6 - direction / 4];
        state->work_height = state->source_base_height_times_two
            + state->source_half_height * ((state->source_slope >> state->source_side_shift) & 3);
        if (state->source_exit_height < state->work_height)
            return 1;
        state->source_ceiling_height = battle_move_calculate_tile_ceiling(state->tile_x, state->tile_y,
            state->source_side_shift, (records[state->source_unit_record_index].standing_height + 5));
        state->source_side_height_delta = 0;
    } else if (state->source_unit_record_flag != 0) {
        if (!(frontier[state->tile_index] & FRONTIER_FLAG_STAND_ON_UNIT))
            return 1;
        state->source_exit_height = records[state->source_unit_record_index].top_height;
        state->source_opposite_height = state->source_exit_height;
        state->source_ceiling_height = battle_move_calculate_tile_ceiling(
            state->tile_x, state->tile_y, state->source_side_shift, (state->source_exit_height - 1));
        state->source_side_height_delta = 0;
    } else {
        state->source_exit_height = state->source_base_height_times_two
            + state->source_half_height * ((state->source_slope >> state->source_side_shift) & 3);
        state->source_opposite_height = state->source_base_height_times_two
            + state->source_half_height * ((state->source_slope >> state->destination_side_shift) & 3);
        state->source_ceiling_height = battle_move_calculate_tile_ceiling(
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
static s32 battle_move_init_destination_geometry(s32 candidate) {
    move_pathfind_scratch_t* config = g_battle_move_config_ptr;
    move_spread_state_t* state = g_battle_move_scratch_pad_ptr;
    u8* tile_flags = g_battle_move_frontier_flags_ptr;
    move_record_t* records = g_battle_move_records_ptr;

    state->destination_unit_record_flag = candidate >= 2;
    state->work_level = candidate & 1;
    if (state->inner_count >= 5)
        state->work_level = config->high_elevation;
    state->destination_index
        = state->work_level * MAP_TILES_PER_LAYER + state->work_y * config->map_max_x + state->work_x;
    state->destination_tile = &g_battle_map_tile_data[state->destination_index];
    state->destination_panel = &g_battle_target_panels[state->destination_index];
    state->destination_base_height_times_two = state->destination_tile->height * 2;
    state->destination_half_height = state->destination_tile->depth_half_height.half_height;
    state->destination_slope = state->destination_tile->slope_type;
    if (state->inner_count >= 5 || state->destination_unit_record_flag != 0) {
        state->destination_unit_record_index = state->destination_panel->unit_record_index;
        if (state->inner_count >= 5) {
            state->destination_unit_record_flag = 1;
            if (!(tile_flags[state->destination_index] & FRONTIER_FLAG_MOUNTABLE_UNIT))
                return 1;
            state->destination_entry_height = records[state->destination_unit_record_index].standing_height
                + g_battle_move_height_offsets[candidate];
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
        state->destination_ceiling_height = battle_move_calculate_tile_ceiling(
            state->work_x, state->work_y, state->destination_side_shift, (state->destination_entry_height - 1));
        state->destination_panel_index = state->destination_unit_record_index + MAP_TILE_COUNT;
        state->effective_destination_panel = &g_battle_target_panels[state->destination_panel_index];
    } else {
        if (!(tile_flags[state->destination_index] & FRONTIER_FLAG_ENTERABLE))
            return 4;
        state->destination_entry_height = state->destination_base_height_times_two
            + state->destination_half_height * ((state->destination_slope >> state->destination_side_shift) & 3);
        state->destination_opposite_height = state->destination_base_height_times_two
            + state->destination_half_height * ((state->destination_slope >> state->source_side_shift) & 3);
        state->destination_side_height_delta = state->destination_entry_height - state->destination_opposite_height;
        state->destination_ceiling_height = battle_move_calculate_tile_ceiling(
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
        state->effective_destination_panel = &g_battle_target_panels[state->destination_panel_index];
    }
    return 0;
}

/* Check a movement candidate and update its mode-dependent budget state.
 *
 * AI mode returns 11 after an accepted update; panel byte 4 breaks equal
 * budgets by the smaller immediate height difference. Non-AI mode returns 0
 * after geometry checks and sets budget_matches only when the budget matches.
 */
static s32 battle_move_update_candidate(s32 extra_span) {
    move_spread_state_t* state = g_battle_move_scratch_pad_ptr;
    move_pathfind_scratch_t* config = g_battle_move_config_ptr;
    u8* terrain_costs = g_battle_move_terrain_costs_ptr;
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
            g_battle_move_frontier_flags_ptr[state->destination_index] |= FRONTIER_FLAG_QUEUED;
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
                - g_battle_move_terrain_costs_ptr[state->current_tile->surface.type] - extra_span)
            state->budget_matches = 1;
    }
    return 0;
}

/* Spread movement to ordinary neighboring candidates.
 *
 * Direction callbacks prepare each candidate; accepted movement updates use
 * zero extra span. Outside AI mode, candidate_saved enables the common finalizer.
 */
static void battle_move_spread_to_neighbors(void) {
    move_pathfind_scratch_t* config = g_battle_move_config_ptr;
    move_spread_state_t* state = g_battle_move_scratch_pad_ptr;
    s32 direction;
    s32 candidate;
    s32 callback;

    battle_move_init_spread_scratch();
    for (direction = 0; direction < state->outer_count; direction++) {
        callback = direction % 4;
        g_battle_move_spread_preset_table[callback]();
        battle_move_set_spreading_tile_x_and_y_coordinates();
        if (battle_move_check_spreading_tile_coordinates() == 0 && battle_move_init_source_geometry(direction) == 0) {
            if (state->inner_count > 4 && (config->x != state->work_x || config->y != state->work_y))
                continue;
            for (candidate = 0; candidate < state->inner_count; candidate++) {
                if (battle_move_init_destination_geometry(candidate) == 0 && battle_move_update_candidate(0) == 0
                    && state->budget_matches != 0)
                    battle_move_save_selected_candidate(direction, candidate, 0);
            }
        }
    }
    if (config->ai_propagation_mode == 0 && state->candidate_saved != 0)
        battle_move_apply_selected_candidate(state);
}

/*
 * Apply the saved movement candidate to the current spreading state.
 *
 * Copy the saved coordinates and residual, then restore half-height offsets
 * when their saved presence bits are set. The destination update uses byte
 * masks because its equivalent bitfield assignments change register allocation.
 */
static void battle_move_apply_selected_candidate(move_spread_state_t* state) {
    state->candidate_remaining_range = state->saved_remaining_range;
    state->tile_x = state->saved_x;
    state->tile_y = state->saved_y;
    state->tile_level = state->saved_level;
    if (state->saved_height_offset_flags & MOVE_SAVED_SOURCE_HEIGHT) {
        move_height_offsets_t* offsets = &g_battle_move_path_height_offsets;
        move_height_offsets_t packed = *offsets;
        packed.source_height_offset = state->saved_source_height_offset;
        packed.source_present = 1;
        *offsets = packed;
    }
    if (state->saved_height_offset_flags & MOVE_SAVED_DESTINATION_HEIGHT) {
        u8* offsets = (u8*)&g_battle_move_path_height_offsets;
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
static void battle_move_calculate_spread(void) {
    move_pathfind_scratch_t* config = g_battle_move_config_ptr;
    move_spread_state_t* state = g_battle_move_scratch_pad_ptr;
    s32 outer_index;
    s32 inner_index;
    s32 pass_index;
    s32 path_index;
    s32 callback_index;

    battle_move_init_spread_scratch();
    for (outer_index = 0; outer_index < state->outer_count; outer_index++) {
        callback_index = outer_index % 4;
        g_battle_move_spread_preset_table[callback_index]();
        if (battle_move_init_source_geometry(outer_index) == 0) {
            pass_index = 1;
            battle_move_set_spreading_tile_x_and_y_coordinates();
            if (config->jump_half != 0) {
                do {
                    if (battle_move_check_spreading_tile_coordinates() == 0) {
                        state->work_x = state->tile_x + state->x_step * (pass_index + 1);
                        state->work_y = state->tile_y + state->y_step * (pass_index + 1);
                        if (battle_move_check_spreading_tile_coordinates() == 0) {
                            for (inner_index = 0; inner_index < state->inner_count; inner_index++) {
                                if (battle_move_init_destination_geometry(inner_index) == 0) {
                                    state->placement_failed = 0;
                                    for (path_index = 1; path_index <= pass_index; path_index++) {
                                        if (battle_move_check_horizontal_jump(
                                                (state->work_y - state->y_step * path_index) * config->map_max_x
                                                    + (state->work_x - state->x_step * path_index),
                                                state->source_side_shift, state->destination_side_shift,
                                                state->destination_entry_height)
                                            != 0) {
                                            state->placement_failed = 1;
                                            break;
                                        }
                                    }
                                    if (state->placement_failed == 0 && battle_move_update_candidate(pass_index) == 0
                                        && state->budget_matches != 0) {
                                        battle_move_save_selected_candidate(outer_index, inner_index, pass_index);
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
        battle_move_apply_selected_candidate(state);
    }
}

/* Rank and save a movement candidate for the common finalizer.
 *
 * Every attempt clears budget_matches; accepted candidates replace the saved
 * coordinates, selectors and predecessor budget, then set candidate_saved.
 */
static void battle_move_save_selected_candidate(s32 direction, s32 candidate, s32 extra_span) {
    move_spread_state_t* state = g_battle_move_scratch_pad_ptr;
    u8 height_delta;

    state->budget_matches = 0;
    if (state->inner_count >= 5) {
        state->saved_remaining_range = state->effective_destination_panel->remaining_range;
    } else {
        state->work_height = state->effective_destination_panel->max_height_delta;
        if (g_battle_move_config_ptr->move_type == MOVE_CLASS_FLY) {
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
        state->saved_source_height_offset = g_battle_move_height_offsets[6 - direction / 4];
    }
    if (state->inner_count >= 5) {
        state->saved_height_offset_flags |= MOVE_SAVED_DESTINATION_HEIGHT;
        state->saved_destination_height_offset = g_battle_move_height_offsets[candidate];
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
void battle_move_calculate_pathing_time_sliced(s32 flags, s32 jump, s32 x, s32 y, s32 level, s32 target_x, s32 target_y,
    s32 target_level, s32 initialize, s32* suspended) {
    battle_move_calculate_pathing(flags, jump, x, y, level, target_x, target_y, target_level, initialize, suspended, 1);
}

/* Falls off the end: move_calculate_pathing leaves the path block pointer in $v0 and
 * the caller consumes it, so the result type belongs on this definition. */
battle_walk_path_t* battle_move_calculate_walkto_pathing(
    s32 flags, s32 jump, s32 x, s32 y, s32 level, s32 target_x, s32 target_y, s32 target_level) {
    s32 suspended;
    battle_move_calculate_pathing(flags, jump, x, y, level, target_x, target_y, target_level, 1, &suspended, 0);
}

/* Pathfinding pass for a generic unit (id 0x7f) from (x, y, level) to the target
 * tile; returns the route buffer, or 0 when either tile is invalid or unreachable.
 *
 * The 0xff record fill is an s32 local: a literal fill becomes a single-set
 * hoisted pseudo that sched1 places after `li s2,0xf`.
 */
static battle_walk_path_t* battle_move_calculate_pathing(s32 flags, s32 jump, s32 x, s32 y, u32 level, s32 target_x,
    s32 target_y, u32 target_level, s32 initialize, s32* suspended, s32 check_budget) {
    u8 frontier_snapshot[MAP_TILE_COUNT];
    move_height_offsets_t no_offsets = { 0 };
    move_pathfind_scratch_t* config = g_battle_move_config_ptr;
    move_spread_state_t* state = g_battle_move_scratch_pad_ptr;
    move_record_t* records = g_battle_move_records_ptr;
    u8* frontier = g_battle_move_frontier_flags_ptr;
    s32 completed_pass = 0;
    s32 i;
    map_tile_t* tile;
    s32 empty_flags;

    *suspended = 0;
    if (initialize) {
        if (x < 0 || x >= g_battle_map_max_x || y < 0 || y >= g_battle_map_max_y || level >= MAP_TERRAIN_LAYER_COUNT
            || target_x < 0 || target_x >= g_battle_map_max_x || target_y < 0 || target_y >= g_battle_map_max_y
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
        config->map_max_x = g_battle_map_max_x;
        config->map_max_y = g_battle_map_max_y;
        g_battle_move_pathing_tile_index = (config->high_elevation << 8) + config->y * config->map_max_x + config->x;
        if (g_battle_map_tile_data[g_battle_move_pathing_tile_index].flags.blocked
            || g_battle_map_tile_data[g_battle_move_pathing_tile_index].flags.untargetable) {
            return 0;
        }
        config->target_index = (config->target_level << 8) + config->target_y * config->map_max_x + config->target_x;
        if (g_battle_map_tile_data[config->target_index].flags.blocked
            || g_battle_map_tile_data[config->target_index].flags.untargetable) {
            return 0;
        }
        config->unit_id = MOVE_GENERIC_UNIT_ID;
        config->will_drown = 1;
        config->will_sink = 1;
        config->fly_or_teleport = 0;
        config->unit_size = 6;
        config->movement_set_3 = 0;
        g_battle_move_weather_terrain_cost = battle_move_get_weather_terrain_cost();
        config->cost_table = MAIN_TERRAIN_COST_TABLE_DEFAULT;
        config->move_type = MOVE_CLASS_NORMAL;
        config->movement_3 = 0;
        g_battle_move_pathing_ignore_terrain = 0;
        g_battle_move_pathing_scratch = config->cost_table;
        for (i = 0; i < 256; i++) {
            g_battle_move_terrain_cost = g_main_terrain_movement_cost_tables[g_battle_move_pathing_scratch][i];
            if (g_battle_move_terrain_cost == MAIN_TERRAIN_COST_WEATHER) {
                g_battle_move_terrain_cost = g_battle_move_weather_terrain_cost;
            }
            if ((flags & MOVE_PATHFIND_IGNORE_TERRAIN_COST) && g_battle_move_pathing_scratch != 0xFF) {
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
        config->move = MOVE_PATHFIND_RANGE;
        config->can_ride = 0;
        g_battle_move_pathing_team = 0;
        config->_unknown_1a = g_battle_unknown_8018f4fc;
        empty_flags = MOVE_RECORD_NONE;
        for (i = MOVE_RECORD_COUNT - 1; i >= 0; i--) {
            records[i].unit_id_flags = empty_flags;
        }
        g_battle_move_pathing_unit_record_count = 0;
        battle_move_set_tile_flags_for_pathfinding(MOVE_TILE_FLAGS_MODE_TERRAIN_ONLY);
        state->outer_count = 4;
        g_battle_target_panels[g_battle_move_pathing_tile_index].remaining_range = config->move + 1;
        g_battle_target_panels[g_battle_move_pathing_tile_index].max_height_delta = 0;
        state->inner_count = 2;
        frontier[g_battle_move_pathing_tile_index] |= FRONTIER_FLAG_QUEUED;
        state->frontier_max_remaining_range = config->move + 1;
        g_battle_move_pathing_resume_pass = 0;
    } else {
        main_mem_copy_byte_data_source_first(
            g_battle_ai_unit_snapshot_storage, getScratchAddr(0), PSX_SCRATCHPAD_BYTE_COUNT);
    }
    for (i = g_battle_move_pathing_resume_pass; i < config->move; i++) {
        if (check_budget && battle_ai_is_pathfinding_budget_exhausted() && completed_pass) {
            g_battle_move_pathing_resume_pass = i;
            *suspended = 1;
            main_mem_copy_byte_data_source_first(
                getScratchAddr(0), g_battle_ai_unit_snapshot_storage, PSX_SCRATCHPAD_BYTE_COUNT);
            return &g_battle_move_path;
        }
        if (!state->frontier_max_remaining_range) {
            break;
        }
        if (g_battle_target_panels[config->target_index].remaining_range >= state->frontier_max_remaining_range) {
            break;
        }
        for (state->tile_level = 0; state->tile_level < MAP_TERRAIN_LAYER_COUNT; state->tile_level++) {
            state->level_offset = state->tile_level * MAP_TILES_PER_LAYER;
            for (state->tile_y = 0; state->tile_y < config->map_max_y; state->tile_y++) {
                state->row_offset = state->level_offset + state->tile_y * config->map_max_x;
                for (state->tile_x = 0; state->tile_x < config->map_max_x; state->tile_x++) {
                    state->tile_index = state->row_offset + state->tile_x;
                    state->current_tile = &g_battle_map_tile_data[state->tile_index];
                    state->current_panel = &g_battle_target_panels[state->tile_index];
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
    for (i = 0; i < MAP_TILE_COUNT; i++) {
        tile = &g_battle_map_tile_data[i];
        tile->ceiling_depth_and_marks.move_destination = 0;
        if (g_battle_target_panels[i].ride_remaining_range != 0
            || ((g_battle_map_tile_data[i].depth_half_height.half_height < 3
                    || !(frontier[i] & FRONTIER_FLAG_LOW_CORNER))
                && g_battle_target_panels[i].remaining_range != 0 && tile->depth_half_height.half_height < 4
                && g_battle_map_tile_data[i].surface.type != MAP_SURFACE_OBSTACLE
                && (tile->depth_half_height.depth == 0 || !config->cannot_stay_on_water)
                && (tile->depth_half_height.depth != 3 || !config->will_drown)
                && (tile->depth_half_height.depth < 4 || !config->will_sink)
                && (tile->surface.type != MAP_SURFACE_LAVA || config->move_type != MOVE_CLASS_FLY)
                && (frontier[i] & FRONTIER_FLAG_ENTERABLE))) {
            state->reachable_count++;
            tile->ceiling_depth_and_marks.move_destination = 1;
        }
    }
    g_battle_move_reachable_tiles_valid = 1;
    g_battle_move_reachable_unit_id = config->unit_id;
    state->tile_x = config->target_x;
    state->tile_y = config->target_y;
    state->tile_level = config->target_level;
    state->selected_source_side_shift = 0;
    g_battle_move_pathing_tile_index
        = (config->target_level << 8) + config->target_y * config->map_max_x + config->target_x;
    if (!(g_battle_map_tile_data[g_battle_move_pathing_tile_index].ceiling_depth_and_marks.move_destination)) {
        return 0;
    }
    g_battle_move_pathing_frontier = frontier;
    battle_target_clear_panel_spread_flags();
    g_battle_move_path_height_offsets = no_offsets;
    g_battle_move_destination_unit_record = 0;
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
        state->current_tile = &g_battle_map_tile_data[state->tile_index];
        state->current_panel = &g_battle_target_panels[state->tile_index];
        state->path_length++;
        g_battle_move_pathing_scratch = state->current_panel->remaining_range;
        state->current_panel->mark = state->path_length;
        state->candidate_remaining_range = g_battle_move_pathing_scratch;
        if (state->tile_x == config->x && state->tile_y == config->y && state->tile_level == config->high_elevation) {
            break;
        }
        battle_move_spread_to_neighbors();
        if (state->candidate_remaining_range == g_battle_move_pathing_scratch) {
            battle_move_calculate_spread();
            if (state->candidate_remaining_range == g_battle_move_pathing_scratch) {
                return 0;
            }
        }
    }
    battle_move_encode_path_steps();
    return &g_battle_move_path;
}

/* Propagate the acting unit's movement budget outward from a target tile.
 *
 * Each pass snapshots the frontier before visiting it. A time-budget pause
 * saves scratchpad state and sets *suspended to 1 while returning 0.
 * Return -1 for an absent unit on initialization, or the reachable count
 * on completion. The caller must check suspension before using that count.
 */
s32 battle_ai_propagate_target_movement(
    s32 unit_id, s32 target_x, s32 target_y, s32 target_elevation, s32 initialize, s32* suspended, s32 check_budget) {
    u8 frontier_snapshot[MAP_TILE_COUNT];
    move_pathfind_scratch_t* config = g_battle_move_config_ptr;
    move_spread_state_t* state = g_battle_move_scratch_pad_ptr;
    move_record_t* records = g_battle_move_records_ptr;
    u8* frontier = g_battle_move_frontier_flags_ptr;
    s32 completed_pass = 0;
    s32 pass;
    s32 index;
    target_panel_t* panel;

    *suspended = 0;
    if (initialize) {
        if (g_battle_unit_stats[unit_id].entd_slot == BATTLE_ENTD_SLOT_NONE)
            return -1;
        battle_move_store_unit_movement_to_scratchpad(unit_id);
        config->move = MOVE_PATHFIND_RANGE;
        for (pass = MOVE_RECORD_COUNT - 1; pass >= 0; pass--)
            records[pass].unit_id_flags = MOVE_RECORD_NONE;
        battle_move_set_tile_flags_for_pathfinding(MOVE_TILE_FLAGS_MODE_START_SPREAD);
        index = (config->high_elevation * MAP_TILES_PER_LAYER) + config->y * config->map_max_x + config->x;
        panel = &g_battle_target_panels[index];
        panel->ride_remaining_range = 0;
        panel->remaining_range = 0;
        frontier[index] &= ~FRONTIER_FLAG_QUEUED;
        state->outer_count = 4;
        state->inner_count = 4;
        panel->remaining_range = 0;
        panel->max_height_delta = 0xff;
        index = (target_elevation * MAP_TILES_PER_LAYER) + target_y * config->map_max_x + target_x;
        panel = &g_battle_target_panels[index];
        panel->remaining_range = MOVE_PATHFIND_RANGE + 1;
        frontier[index] |= FRONTIER_FLAG_QUEUED;
        panel->max_height_delta = 0;
        g_battle_ai_propagation_resume_pass = 0;
        config->ai_propagation_mode = 1;
    } else {
        /* A direct scratchpad address avoids an extra linker-address addiu. */
        main_mem_copy_byte_data_source_first(
            g_battle_ai_unit_snapshot_storage, getScratchAddr(0), PSX_SCRATCHPAD_BYTE_COUNT);
    }
    if (config->move_type == MOVE_CLASS_TELEPORT) {
        /* Teleport and Teleport 2 bypass propagation, not destination filtering. */
        u8 remaining_range = 1;
        target_panel_t* fill_panels = g_battle_target_panels;
        for (pass = MAP_TILE_COUNT - 1; pass >= 0; pass--)
            fill_panels[pass].remaining_range = remaining_range;
    } else {
        for (pass = g_battle_ai_propagation_resume_pass; pass < config->move; pass++) {
            if (check_budget && battle_ai_is_pathfinding_budget_exhausted() && completed_pass) {
                g_battle_ai_propagation_resume_pass = pass;
                *suspended = 1;
                main_mem_copy_byte_data_source_first(
                    getScratchAddr(0), g_battle_ai_unit_snapshot_storage, PSX_SCRATCHPAD_BYTE_COUNT);
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
                        state->current_tile = &g_battle_map_tile_data[state->tile_index];
                        state->current_panel = &g_battle_target_panels[state->tile_index];
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

static void battle_target_check_moldball_virus_depth(s16 ability_id) {
    map_tile_t* tile;
    target_panel_t* panel;
    s32 i;

    if (ability_id != ABILITY_ID_MONSTER_GREAT_MORBOL_MOLDBALL_VIRUS)
        return;
    i = 0;
    panel = g_battle_target_panel_data;
    tile = g_battle_map_tile_data;
    do {
        if (tile->depth_half_height.depth) {
            panel->remaining_range = 0;
        }
        tile++;
        i += 1;
        panel++;
    } while (i < 0x200);
}

/* Clear the range and the mark of each panel whose tile is flagged untargetable.
 * Only the 256 lower-level panels are visited. */
static void battle_target_clear_panels_on_untargetable_tiles(void) {
    s32 i = 0;
    map_tile_t* src;
    target_panel_t* dst = g_battle_target_panel_data;

    src = g_battle_map_tile_data;
    for (; i < 256; src++, i++, dst++) {
        if (src->flags.untargetable) {
            dst->remaining_range = 0;
            dst->mark = 0;
        }
    }
}

/* Keeps, per map position, only the terrain layer nearest a reference height.
 *
 * Heights are in half units (height * 2 + slope half-height + depth * 2). A
 * blocked tile clears its panel and counts as height -255. With single_layer
 * set, only the higher layer is considered. The nearer layer survives when it
 * lies within tolerance * 2 of the reference; the other panel is cleared.
 *
 * The upper layer index is a separate variable (j): writing [i + 0x100] lets
 * the front end fold the 0x100 into the symbol, and the target instead
 * strength-reduces 5 * j and 8 * j against hoisted table bases. Block-scoped
 * depth temporaries keep that value out of global allocation. */
static void battle_target_apply_vertical_tolerance(u8 ref_height, u8 tolerance, s32 single_layer) {
    s32 max_delta = tolerance * 2;
    s32 ref = ref_height;
    s32 i;
    s32 j;
    s32 lower;
    s32 upper;
    s32 lower_delta;
    s32 upper_delta;
    map_tile_t* tile;
    target_panel_t* lower_panel;
    target_panel_t* upper_panel;

    for (i = 0; i < 0x100; i++) {
        tile = &g_battle_map_tile_data[i];
        lower_panel = &g_battle_target_panel_data[i];
        if (!tile->flags.blocked) {
            map_tile_depth_t depth = tile->depth_half_height;
            lower = tile->height * 2 + depth.half_height + depth.depth * 2;
        } else {
            lower = -0xFF;
            lower_panel->remaining_range = 0;
            lower_panel->mark = 0;
        }
        j = i + 0x100;
        tile = &g_battle_map_tile_data[j];
        upper_panel = &g_battle_target_panel_data[j];
        if (!tile->flags.blocked) {
            map_tile_depth_t depth = tile->depth_half_height;
            upper = tile->height * 2 + depth.half_height + depth.depth * 2;
        } else {
            upper = -0xFF;
            upper_panel->remaining_range = 0;
            upper_panel->mark = 0;
        }
        if (single_layer != 0) {
            if (lower >= upper) {
                upper = -0xFF;
            } else {
                lower = -0xFF;
            }
        }
        if (lower >= ref) {
            lower_delta = lower - ref;
        } else {
            lower_delta = ref - lower;
        }
        if (upper >= ref) {
            upper_delta = upper - ref;
        } else {
            upper_delta = ref - upper;
        }
        if (upper_delta < lower_delta) {
            lower_panel->remaining_range = 0;
            lower_panel->mark = 0;
            if (max_delta < upper_delta) {
                upper_panel->remaining_range = 0;
                upper_panel->mark = 0;
            }
        } else {
            upper_panel->remaining_range = 0;
            upper_panel->mark = 0;
            if (max_delta < lower_delta) {
                lower_panel->remaining_range = 0;
                lower_panel->mark = 0;
            }
        }
    }
}

/* Clear the mark byte (the spread frontier) of all 512 targeting panels. */
void battle_target_clear_panel_spread_flags(void) {
    s32 i;
    target_panel_t* panel;

    for (i = 511, panel = &g_battle_target_panels[511]; i >= 0; i--, panel--) {
        panel->mark = 0;
    }
}

/* Remove the range from every panel, on both levels, within `range` tiles
 * (Manhattan distance) of (x, y). */
static void battle_target_remove_close_range(s32 x, s32 y, s32 range) {
    s32 i;
    s32 j;
    s32 dy;
    s32 dx;
    s32 k;

    for (i = 0; i < g_battle_map_max_y; i++) {
        dy = (y < i) ? (i - y) : (y - i);
        for (j = 0; j < g_battle_map_max_x; j++) {
            dx = (x < j) ? (j - x) : (x - j);
            if (range >= (dy + dx)) {
                g_battle_target_panel_data[(i * g_battle_map_max_x) + j].remaining_range = 0;
                k = j + 0x100;
                g_battle_target_panel_data[(i * g_battle_map_max_x) + k].remaining_range = 0;
            }
        }
    }
}

/* Repeat the targeting-panel spread pass up to the given count.
 *
 * Each pass visits every panel whose mark byte flags it as a new frontier,
 * clears the mark and spreads from it; passes stop once none spread. A zero
 * count clears the tile data instead. Every caller passes 0 as the unused
 * second argument. */
static void battle_target_spread_panels(u8 passes, s32 unused) {
    s32 y;
    s32 x;
    s32 changed;
    s32 pass;
    target_panel_t* panel;

    if (passes == 0) {
        battle_target_clear_panel_spread_flags();
        return;
    }
    changed = 1;
    for (pass = 0; pass < passes; pass++) {
        if (changed == 0) {
            break;
        }
        changed = 0;
        for (y = 0; y < g_battle_map_max_y; y++) {
            for (x = 0; x < g_battle_map_max_x; x++) {
                panel = &g_battle_target_panels[y * g_battle_map_max_x + x];
                if (panel->mark != 0) {
                    panel->mark = 0;
                    changed += battle_target_spread_panel_to_neighbors(y, x);
                }
            }
        }
    }
}

/* Spread one targeting panel's remaining range to its four orthogonal
 * neighbors. A neighbor whose residual budget is lower than this panel's is
 * refreshed to one less, on both map levels, and marked as a new frontier when
 * enough budget is left to keep spreading. Returns 1 when any neighbor was
 * marked. */
static s32 battle_target_spread_panel_to_neighbors(s32 y, s32 x) {
    target_panel_t* panel;
    s32 changed = 0;
    s32 i;
    s32 dx;
    s32 dy;
    s32 next_x;
    s32 next_y;
    s32 index;
    s32 upper_x;
    s32 range = g_battle_target_panels[y * g_battle_map_max_x + x].remaining_range;
    s32 spread_range = range - 1;

    for (i = 0; i < 4; i++) {
        switch (i) {
        case 0:
            dx = 1;
            dy = 0;
            break;
        case 1:
            dx = 0;
            dy = 1;
            break;
        case 2:
            dx = -1;
            dy = 0;
            break;
        case 3:
            dx = 0;
            dy = -1;
            break;
        }
        next_x = x + dx;
        next_y = y + dy;
        if ((next_x >= 0) && (next_y >= 0) && (next_x < g_battle_map_max_x) && (next_y < g_battle_map_max_y)) {
            index = next_y * g_battle_map_max_x + next_x;
            panel = &g_battle_target_panels[index];
            if (panel->remaining_range < range) {
                panel->remaining_range = spread_range;
                upper_x = next_x + 0x100;
                g_battle_target_panels[next_y * g_battle_map_max_x + upper_x].remaining_range = spread_range;
                if (range >= 3) {
                    panel->mark = 1;
                    changed = 1;
                }
            }
        }
    }
    return changed;
}

/* Compute unit height after mount, Float, and water movement adjustments.
 *
 * Float and BATTLE_MOVEMENT_SET_3_MOVE_IN_WATER stand on the water surface;
 * BATTLE_MOVEMENT_SET_3_WALK_ON_WATER stands one level below it. */
static u8 battle_unit_get_effective_height(const unit_stats_t* unit) {
    map_tile_t* tile;
    map_tile_depth_t terrain_depth;
    s32 height;
    u8 effective_height;
    u8 water_depth;
    u8 ignores_water = 0;
    u8 flags;

    tile = &g_battle_map_tile_data[battle_map_calculate_location(unit)];
    terrain_depth = tile->depth_half_height;
    flags = unit->mount_info;
    height = tile->height * 2 + (terrain_depth.half_height);
    water_depth = tile->depth_half_height.depth * 2;
    effective_height = height;
    if (flags & BATTLE_MOUNT_INFO_FLAG_RIDER) {
        unit = &g_battle_unit_stats[flags & 0x1f];
        height += 2;
        effective_height = height;
    }
    flags = unit->ability_flags.movement[2];
    if ((flags & BATTLE_MOVEMENT_SET_3_FLOAT)
        || (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FLOAT)] & STATUS_BYTE_MASK(STATUS_ID_FLOAT))) {
        effective_height += 2;
        ignores_water = 1;
    }
    if (water_depth != 0) {
        if (ignores_water || (flags & BATTLE_MOVEMENT_SET_3_MOVE_IN_WATER)) {
            effective_height += water_depth;
        } else if (flags & BATTLE_MOVEMENT_SET_3_WALK_ON_WATER) {
            effective_height += water_depth - 2;
        }
    }
    return effective_height;
}

/*
 * Mark the longbow (arc) targeting panels: every tile at least three steps
 * away whose distance, less a quarter of the unit's height advantage, is
 * within range. Both elevation levels of each tile are tested.
 */
static void battle_target_calculate_arc_range(const unit_stats_t* unit, u8 range, u8 flags) {
    u8 height = battle_unit_get_effective_height(unit);
    u8 tile_height;
    s32 ux;
    s32 uy;
    s32 x;
    s32 y;
    s32 dy;
    s32 dist;
    s32 level;
    s32 index;
    map_tile_t* tile;

    ux = unit->x;
    uy = unit->position.y;
    for (y = 0; y < g_battle_map_max_y; y++) {
        if (uy < y) {
            dy = y - uy;
        } else {
            dy = uy - y;
        }
        for (x = 0; x < g_battle_map_max_x; x++) {
            if (ux < x) {
                dist = x - ux + dy;
            } else {
                dist = ux - x + dy;
            }
            if (dist >= 3) {
                for (level = 0; level < 2; level++) {
                    index = (level << 8) + y * g_battle_map_max_x + x;
                    tile = &g_battle_map_tile_data[index];
                    tile_height
                        = tile->height * 2 + tile->depth_half_height.half_height + tile->depth_half_height.depth * 2;
                    if (dist - (height - tile_height) / 4 <= range) {
                        g_battle_target_panels[index].remaining_range = 1;
                    }
                }
            }
        }
    }
}

/*
 * Mark the targeting panels in range of the unit's weapon.
 *
 * Frog uses the bare-hand entry; an empty right hand falls back to the left
 * hand, and non-weapon items also use entry 0. Striking, lunging, and direct
 * weapon flags select the special range shapes; direct also removes the close
 * range.
 */
static void battle_target_calculate_weapon_range(const unit_stats_t* unit) {
    s32 y;
    s32 x;
    s32 index;
    u8 weapon;
    u8 range;
    u8 flags;
    weapon_data_t* data;
    target_panel_t* panel;
    target_panel_t* upper;

    y = unit->position.y;
    weapon = unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON];
    x = unit->x;
    index = x + y * g_battle_map_max_x;
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FROG)] & STATUS_BYTE_MASK(STATUS_ID_FROG)) {
        weapon = ITEM_ID_NOTHING;
    }
    if (weapon == ITEM_ID_NONE) {
        weapon = unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON];
        if (weapon == ITEM_ID_NONE) {
            weapon = ITEM_ID_NOTHING;
        }
    }
    if (weapon >= ITEM_ID_SHIELD_FIRST) {
        weapon = ITEM_ID_NOTHING;
    }
    data = &g_main_item_weapon_data[weapon];
    range = data->range;
    flags = data->flags;
    panel = &g_battle_target_panels[index];
    upper = &g_battle_target_panels[index + 0x100];
    panel->remaining_range = range + 1;
    upper->remaining_range = range + 1;
    panel->mark = 1;
    if (flags & (WEAPON_FLAG_STRIKING | WEAPON_FLAG_LUNGING)) {
        battle_target_calculate_strike_lunge_range(unit, flags);
    } else if (flags & WEAPON_FLAG_DIRECT) {
        battle_target_spread_panels(range, 0);
        battle_target_remove_close_range(x, y, 2);
    } else {
        battle_target_calculate_arc_range(unit, range, flags);
    }
    battle_target_clear_range_on_untargetable_tiles();
}

/*
 * Mark the special weapon-range shapes selected by weapon flags 0xc0: flag
 * 0x80 covers the four adjacent tiles within a narrow height band, otherwise
 * the two rings at distance 1 and 2 use a band that widens with distance.
 */
static void battle_target_calculate_strike_lunge_range(const unit_stats_t* unit, u8 flags) {
    u8 height = battle_unit_get_effective_height(unit);
    s32 x;
    s32 y;
    s32 i;
    s32 lo;
    s32 hi;

    x = unit->x;
    y = unit->position.y;
    if (flags & 0x80) {
        lo = height - 6;
        hi = height + 5;
        battle_target_calculate_aoe_vertical_tolerance(x - 1, y, lo, hi);
        battle_target_calculate_aoe_vertical_tolerance(x + 1, y, lo, hi);
        battle_target_calculate_aoe_vertical_tolerance(x, y - 1, lo, hi);
        battle_target_calculate_aoe_vertical_tolerance(x, y + 1, lo, hi);
    } else {
        for (i = 1; i < 3; i++) {
            lo = height + (i - 5) * 2;
            hi = height + 7;
            battle_target_calculate_aoe_vertical_tolerance(x - i, y, lo, hi);
            battle_target_calculate_aoe_vertical_tolerance(x + i, y, lo, hi);
            battle_target_calculate_aoe_vertical_tolerance(x, y - i, lo, hi);
            battle_target_calculate_aoe_vertical_tolerance(x, y + i, lo, hi);
        }
    }
}

/* Give range 1 to the panels of map position (x, y), on either level, whose
 * height lies in lo..hi. Heights are in half steps (height * 2 + slope half
 * height + depth * 2); a negative lo counts as 0 and a position off the map is
 * ignored. The strike and lunge weapon ranges are built from it. */
static void battle_target_calculate_aoe_vertical_tolerance(s32 x, s32 y, s32 lo, s32 hi) {
    s32 level;
    s32 idx;
    s32 height;
    map_tile_t* tile;
    target_panel_t* panel;
    map_tile_depth_t flags;

    if ((x >= 0) && (x < g_battle_map_max_x) && (y >= 0) && (y < g_battle_map_max_y)) {
        level = 0;
        if (lo < 0) {
            lo = 0;
        }
        do {
            idx = (level << 8) + (y * g_battle_map_max_x) + x;
            tile = &g_battle_map_tile_data[idx];
            panel = &g_battle_target_panel_data[idx];
            flags = tile->depth_half_height;
            height = (tile->height * 2) + (flags.half_height) + ((flags.depth) * 2);
            level++;
            if ((height >= lo) && (hi >= height)) {
                panel->remaining_range = 1;
            }
        } while (level < 2);
    }
}

/* Remove the range from every panel, on both levels, whose tile is flagged
 * untargetable. */
static void battle_target_clear_range_on_untargetable_tiles(void) {
    s32 i = 0;
    target_panel_t* dst = g_battle_target_panel_data;
    map_tile_t* src = g_battle_map_tile_data;
    for (; i < 0x200; i++) {
        if (src->flags.untargetable)
            dst->remaining_range = 0;
        src++;
        dst++;
    }
}

/* Mark every tile the acting unit's Jump abilities can reach.
 *
 * Horizontal range and vertical reach start at 1/2 (8/0x10 for job 0x5d) and
 * grow to the best learned Jump record; panels are spread from the unit's
 * tile, and each map position keeps the higher of its two layers when that
 * layer lies within the vertical window. Returns the number of tiles marked,
 * or 0 when the other layer at the unit's position is taller and passable.
 *
 * The target initializes the lower-layer panel giv first (t1 = panels,
 * t2 = t1 + 0x500). That needs `upper_panels` derived from `panels` and the
 * per-tile panel pointers taken upper layer first, because loop.c keeps its
 * giv list in reverse discovery order. */
static s32 battle_target_set_jump_ability_panels(const action_command_t* source) {
    u16 abilities[0x14];
    u8 mp_costs[0x18];
    u8 ability_ct[0x48];
    u8 usable[0x18];
    u8 turns[0x18];
    action_command_t action;
    unit_stats_t* unit;
    map_tile_t* tile;
    map_tile_t* upper;
    map_tile_t* lower;
    target_panel_t* panel;
    target_panel_t* other_panel;
    target_panel_t* panels;
    target_panel_t* upper_panels;
    s32 count;
    s32 i;
    u8 jump_vertical;
    s32 other_height;
    s32 jump_index;
    u16 ability_id;
    main_ability_jump_record_t* jump;
    s32 location;
    s32 other_location;
    s16 height;
    u32 tile_height;
    s16 max_height;
    s32 marked;
    s16 min_height;
    s16 upper_height;
    s16 lower_height;
    u8 range;
    u8 vertical;

    range = 1;
    main_util_copy_action_data(source, &action);
    unit = &g_battle_unit_stats[action.actor_id];
    vertical = 2;
    if (unit->job_id == JOB_ID_MIME) {
        range = 8;
        vertical = 0x10;
    }
    location = battle_map_calculate_location(unit);
    tile = &g_battle_map_tile_data[location];
    tile_height = tile->height;
    height = tile->depth_half_height.half_height + tile_height * 2;
    count = battle_menu_get_unit_skillset_ability_data(
        action.actor_id, action.skillset_id, (s16*)abilities, mp_costs, ability_ct, 0, usable, turns);
    for (i = 0; i < count; i++) {
        ability_id = abilities[i];
        if (ability_id >= ABILITY_ID_JUMP_FIRST && ability_id < ABILITY_ID_CHARGE_FIRST) {
            jump_index = ability_id - ABILITY_ID_JUMP_FIRST;
            jump = &g_main_jump_ability_data[(u16)jump_index];
            jump_vertical = jump->vertical * 2;
            if (range < jump->range) {
                range = jump->range;
            }
            if (jump_vertical > vertical) {
                vertical = jump_vertical;
            }
        }
    }
    min_height = height - vertical;
    max_height = height + vertical;
    if (min_height < 0) {
        min_height = 0;
    }
    battle_target_clear_panel_data();
    if (location < 0x100) {
        other_location = location + 0x100;
    } else {
        other_location = location - 0x100;
    }
    panel = &g_battle_target_panels[location];
    other_panel = &g_battle_target_panels[other_location];
    panel->remaining_range = range + 1;
    other_panel->remaining_range = range + 1;
    if (location < 0x100) {
        panel->mark = 1;
    } else {
        other_panel->mark = 1;
    }
    battle_target_spread_panels(range, 0);
    tile = &g_battle_map_tile_data[other_location];
    panel->remaining_range = 0;
    other_panel->remaining_range = 0;
    other_height = tile->depth_half_height.half_height + tile->height * 2;
    if ((tile->flags.blocked) == 0 && height < other_height) {
        return 0;
    }
    marked = 0;
    panels = g_battle_target_panels;
    upper_panels = panels + 0x100;
    for (i = 0; i < 0x100; i++) {
        other_panel = &upper_panels[i];
        panel = &panels[i];
        lower = &g_battle_map_tile_data[i];
        upper = &g_battle_map_tile_data[i + 0x100];
        if (panel->remaining_range == 0 || (lower->flags.blocked)) {
            lower_height = -1;
        } else {
            lower_height = lower->depth_half_height.half_height + lower->height * 2;
        }
        if (other_panel->remaining_range == 0 || (upper->flags.blocked)) {
            upper_height = -1;
        } else {
            upper_height = upper->depth_half_height.half_height + upper->height * 2;
        }
        if (lower_height < upper_height) {
            tile = lower;
            lower_height = upper_height;
            lower = upper;
            upper = tile;
        }
        if (lower_height >= min_height && max_height >= lower_height && (lower->flags.untargetable) == 0) {
            marked++;
            lower->ceiling_depth_and_marks.ability_range = 1;
        } else {
            lower->ceiling_depth_and_marks.ability_range = 0;
        }
        upper->ceiling_depth_and_marks.ability_range = 0;
    }
    return marked;
}

/* Spread an ability range from the unit's tile and mark the reachable panels.
 *
 * Range bits 0-6 seed the unit's panel on both map levels and bit 7 excludes
 * the unit's own tile afterwards. Panels flagged in flags bits 0-1 are never
 * marked. Returns the number of marked panels. */
static s32 battle_target_spread_ability_range(const unit_stats_t* unit, u8 range) {
    /* A byte collision test retains the original single-mask branch. */
    target_panel_t* panel;
    target_panel_t* upper;
    target_panel_t* panels;
    map_tile_t* tile;
    s32 index;
    s32 i;
    s32 count;
    u8 remaining;

    index = unit->position.y * g_battle_map_max_x + unit->x;
    battle_target_clear_panel_data();
    remaining = (range & 0x7f) + 1;
    panel = &g_battle_target_panels[index];
    upper = &g_battle_target_panels[index + 0x100];
    panel->remaining_range = remaining;
    upper->remaining_range = remaining;
    panel->mark = 1;
    battle_target_spread_panels(range, 0);
    if (range & 0x80) {
        panel->remaining_range = 0;
        upper->remaining_range = 0;
    }
    count = 0;
    tile = g_battle_map_tile_data;
    for (i = 0, panels = g_battle_target_panels; i < 0x200; i++) {
        if (panels->remaining_range != 0 && !(((*(u8*)&tile[i].flags) & MAP_TILE_COLLISION_MASK))) {
            tile[i].ceiling_depth_and_marks.ability_range = 1;
            count++;
        } else {
            tile[i].ceiling_depth_and_marks.ability_range = 0;
        }
        panels++;
    }
    return count;
}

/*
 * Seed the targeting panels for the action in `source` and apply the ability's
 * range, vertical tolerance and targeting flags. Returns the number of tiles
 * the final pass marks as in ability range.
 *
 * The separate byte offset and register binding preserve the target's copy
 * of the location index. Direct typed indexing removes that copy and changes
 * the later register allocation; the binding emits no instructions.
 */
static s32 battle_target_set_ability_panels(const action_command_t* source) {
    action_command_t action;
    unit_stats_t* unit;
    ability_secondary_data_t* ability;
    target_panel_t* panels;
    target_panel_t* origin;
    target_panel_t* seed;
    target_panel_t* seed_upper;
    map_tile_t* tile;
    map_tile_depth_t terrain_depth;
    s32 location;
    s32 index;
    s32 elevation;
    /* Pin: unpinned, GCC computes the origin offset in $v0 instead of $v1. */
    register s32 origin_offset __asm__("$3");
    s32 i;
    u8 range;
    u8 aoe;
    u8 vertical;
    u8 flags_1;
    u8 flags_4;
    s32 x;
    u8 y;

    main_util_copy_action_data(source, &action);
    unit = &g_battle_unit_stats[action.actor_id];
    ability = &g_main_ability_range_data[action.ability_id];
    range = ability->range;
    aoe = ability->aoe;
    flags_1 = ability->flags_1;
    vertical = ability->vertical;
    flags_4 = ability->flags_4;
    x = unit->x;
    y = unit->position.y;
    location = battle_map_calculate_location(unit);
    tile = &g_battle_map_tile_data[location];
    terrain_depth = tile->depth_half_height;
    elevation = tile->height * 2 + (terrain_depth.half_height) + tile->depth_half_height.depth * 2;
    panels = g_battle_target_panels;
    origin_offset = location * sizeof(target_panel_t);
    origin = (target_panel_t*)((s32)panels + origin_offset);
    index = y * g_battle_map_max_x + x;
    if (range == 0xff) {
        for (i = 0; i < 0x200; i++) {
            panels[i].remaining_range = 1;
            panels[i].mark = 0;
        }
    } else {
        for (i = 0; i < 0x200; i++) {
            panels[i].remaining_range = 0;
            panels[i].mark = 0;
        }
        seed = &g_battle_target_panels[index];
        seed_upper = &g_battle_target_panels[index + 0x100];
        seed->remaining_range = range + 1;
        seed_upper->remaining_range = range + 1;
        seed->mark = 1;
        if (flags_1 & ABILITY_SECONDARY_FLAG_1_WEAPON_RANGE) {
            battle_target_calculate_weapon_range(unit);
            seed->remaining_range = 0;
            seed_upper->remaining_range = 0;
        } else {
            battle_target_spread_panels(range, 0);
        }
    }
    if (flags_1 & ABILITY_SECONDARY_FLAG_1_CANNOT_TARGET_SELF) {
        origin->remaining_range = 0;
    }
    if (flags_1 & ABILITY_SECONDARY_FLAG_1_VERTICAL_FIXED) {
        location = x;
        battle_target_apply_vertical_fixed(location, y);
    }
    if (flags_1 & ABILITY_SECONDARY_FLAG_1_VERTICAL_TOLERANCE) {
        battle_target_apply_vertical_tolerance(elevation, vertical, 0);
    }
    if (aoe == 0 || (flags_4 & ABILITY_SECONDARY_FLAG_4_DIRECT_TARGETING)) {
        battle_target_clear_panels_on_untargetable_tiles();
    }
    if (flags_1 & (ABILITY_SECONDARY_FLAG_1_ENEMY_UNIT_PANELS | ABILITY_SECONDARY_FLAG_1_ALLY_UNIT_PANELS)) {
        battle_target_set_state_for_all_unit_panels(unit, flags_1);
    }
    if (flags_1
        & (ABILITY_SECONDARY_FLAG_1_VERTICAL_FIXED | ABILITY_SECONDARY_FLAG_1_ENEMY_UNIT_PANELS
            | ABILITY_SECONDARY_FLAG_1_ALLY_UNIT_PANELS)) {
        return battle_target_clear_selection_state_of_all_panels();
    }
    return battle_target_set_all_panels_targeted_if_targetable();
}

/* Build the targeting panels for a weapon attack: the weapon's range around the
 * acting unit, without the unit's own tile. Returns the number of tiles in range. */
static s32 battle_target_set_weapon_attack_panels(const action_command_t* source) {
    action_command_t action;
    unit_stats_t* unit;
    s32 idx;

    main_util_copy_action_data(source, &action);
    unit = &g_battle_unit_stats[action.actor_id];
    idx = unit->position.y * g_battle_map_max_x + unit->x;
    battle_target_clear_panel_data();
    battle_target_calculate_weapon_range(unit);
    {
        target_panel_t* p1;
        target_panel_t* p2;
        p1 = &g_battle_target_panel_data[idx];
        p2 = &g_battle_target_panel_data[idx + 0x100];
        p1->remaining_range = 0;
        p2->remaining_range = 0;
        return battle_target_set_all_panels_targeted_if_targetable();
    }
}

/* Publish the panels in range to the map: a tile whose panel has range left gets
 * MAP_TILE_FLAG_ABILITY_RANGE unless it is blocked or a cross-section surface,
 * and every other tile loses the flag. Returns the number of flagged tiles. */
static s32 battle_target_set_all_panels_targeted_if_targetable(void) {
    map_tile_t* tile;
    target_panel_t* panel;
    s32 count = 0;
    s32 i;

    for (i = 0; i < MAP_TILE_COUNT; i++) {
        tile = &g_battle_map_tile_data[i];
        panel = &g_battle_target_panel_data[i];
        if (panel->remaining_range != 0 && !(tile->flags.blocked)
            && (tile->surface.type) != MAP_SURFACE_CROSS_SECTION) {
            count++;
            tile->ceiling_depth_and_marks.ability_range = 1;
        } else {
            tile->ceiling_depth_and_marks.ability_range = 0;
        }
    }
    return count;
}

/* Publish the marked panels to the map: a tile whose panel mark is set gets
 * MAP_TILE_FLAG_ABILITY_RANGE unless it is blocked or a cross-section surface,
 * and every other tile loses the flag. Returns the number of flagged tiles.
 *
 * target_set_ability_panels ends with it for abilities that have the
 * vertical-fixed or a unit-panels flag; the others end with
 * target_set_all_panels_targeted_if_targetable, which tests the panel's
 * range instead of its mark. */
static s32 battle_target_clear_selection_state_of_all_panels(void) {
    map_tile_t* tile;
    target_panel_t* panel;
    s32 count = 0;
    s32 i;

    for (i = 0; i < MAP_TILE_COUNT; i++) {
        tile = &g_battle_map_tile_data[i];
        panel = &g_battle_target_panel_data[i];
        if (panel->mark != 0 && !(tile->flags.blocked) && (tile->surface.type) != MAP_SURFACE_CROSS_SECTION) {
            count++;
            tile->ceiling_depth_and_marks.ability_range = 1;
        } else {
            tile->ceiling_depth_and_marks.ability_range = 0;
        }
    }
    return count;
}

/*
 * Marks the targeting panel under `unit` for each simulation slot.
 * The retail loop never advances the unit pointer, so it re-marks the same
 * panel; the shape is kept as compiled.
 */
static void battle_target_set_state_for_all_unit_panels(const unit_stats_t* unit, s32 flags) {
    target_panel_t* panel;
    s32 i;
    s32 mark_allies = flags & ABILITY_SECONDARY_FLAG_1_ALLY_UNIT_PANELS;
    s32 mark_enemies = flags & ABILITY_SECONDARY_FLAG_1_ENEMY_UNIT_PANELS;
    s32 team_diff;
    s16 team_diff_copy;
    s32 team = unit->team_flags;

    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        if (unit->entd_slot != BATTLE_ENTD_SLOT_NONE) {
            team_diff = team ^ unit->initial_team_flags;
            team_diff_copy = team_diff;
            panel = &g_battle_target_panel_data[battle_map_calculate_location(unit)];
            if (mark_allies != 0 && (team_diff & BATTLE_TEAM_MASK) == 0) {
                panel->mark = 1;
            }
            if (mark_enemies != 0 && (team_diff_copy & BATTLE_TEAM_MASK) != 0) {
                panel->mark = 1;
            }
        }
    }
}

/* Mark the horizontal and vertical targeting runs on both map layers.
 *
 * The target reads the first layer's availability byte unsigned and marks
 * both layers when that byte is nonzero. Sharing the loop index and panel
 * temporaries across the sweeps preserves the target register allocation. */
static void battle_target_apply_vertical_fixed(s32 x, s32 y) {
    s32 shared;
    s32 vertical;
    s32 tile_index;
    target_panel_t* panel;
    s32 i;

    x &= 0xFF;
    y &= 0xFF;
    {
        vertical = y;
        for (i = -0x1F; i < 0x20; i++) {
            s32 mx;
            target_panel_t* base = g_battle_target_panel_data;
            target_panel_t* lvl1 = base + 0x100;
            shared = x + i;
            mx = g_battle_map_max_x;
            tile_index = (vertical * mx) + shared;
            panel = &base[tile_index];
            if ((shared >= 0) && (shared < mx) && (panel->remaining_range != 0)) {
                panel->mark = 1;
                lvl1[tile_index].mark = 1;
            }
        }
    }
    {
        shared = x;
        for (i = -0x1F; i < 0x20; i++) {
            target_panel_t* base = g_battle_target_panel_data;
            target_panel_t* lvl1 = base + 0x100;
            vertical = y + i;
            tile_index = (vertical * g_battle_map_max_x) + shared;
            panel = &base[tile_index];
            if ((vertical >= 0) && (vertical < g_battle_map_max_y) && (panel->remaining_range != 0)) {
                panel->mark = 1;
                lvl1[tile_index].mark = 1;
            }
        }
    }
}

/*
 * The .ld places .rodata at 0x80174068 so the switch's jump table lands on the
 * target's table.
 */

/* Build the targeting panels for an action and report how its target is chosen.
 *
 * The skillset's menu type picks the range. Abilities (the default and monster
 * menus, the Elemental ability of the unit's tile, the Draw Out of the chosen
 * katana) use their range data through target_set_ability_panels. Item
 * reaches 1 tile, or 4 with Throw Item or for a Mime. Throw reaches the unit's
 * Move and excludes its own tile. Attack and Charge use the weapon's range, Jump
 * the unit's Jump abilities.
 *
 * Returns a target_panels_e value: INVALID for an absent unit, a katana or
 * ability id out of range or a menu type without an action; NO_TARGETING for the
 * remaining menu types and for an auto ability with range 0 that does not use the
 * weapon's range; NONE when no tile is marked; TILE_ONLY when the action cannot
 * follow a unit (always for Item, Throw, Attack, Charge and Jump); otherwise
 * UNIT_OR_TILE. */
s32 battle_target_set_panels_for_action(const action_command_t* source) {
    action_command_t action;
    unit_stats_t* unit;
    ability_secondary_data_t* ability;
    s32 mode;
    u8 range;
    s32 result;
    u8 flags;
    u8 item;
    u16 ability_id;

    main_util_copy_action_data(source, &action);
    mode = 0;
    if (action.actor_id >= BATTLE_UNIT_SLOT_COUNT) {
        return TARGET_PANELS_INVALID;
    }
    unit = &g_battle_unit_stats[action.actor_id];
    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
        return TARGET_PANELS_INVALID;
    }
    switch (g_main_action_menu_types_by_skillset[action.skillset_id]) {
    case ACTION_MENU_TYPE_DEFAULT:
    case ACTION_MENU_TYPE_MONSTER:
        ability_id = action.ability_id;
        mode = 1;
        break;
    case ACTION_MENU_TYPE_ITEM_INVENTORY:
        range = ((unit->ability_flags.support[2] & BATTLE_SUPPORT_SET_3_THROW_ITEM) || unit->job_id == JOB_ID_MIME) ? 4
                                                                                                                    : 1;
        mode = 2;
        break;
    case ACTION_MENU_TYPE_WEAPON_INVENTORY:
        mode = 2;
        range = unit->move | 0x80;
        break;
    case ACTION_MENU_TYPE_ELEMENTS:
        ability_id = g_main_terrain_geomancy_abilities[g_battle_map_tile_data[battle_map_calculate_location(unit)]
                .surface.type];
        mode = 1;
        break;
    case ACTION_MENU_TYPE_KATANA_INVENTORY:
        mode = 1;
        item = action.item_or_equipment;
        if (item < ITEM_ID_ASURA_KNIFE || item > ITEM_ID_CHIRIJIRADEN) {
            return TARGET_PANELS_INVALID;
        }
        ability_id = (u8)action.item_or_equipment + (ABILITY_ID_DRAW_OUT_ASURA - ITEM_ID_ASURA_KNIFE);
        break;
    case ACTION_MENU_TYPE_ATTACK:
    case ACTION_MENU_TYPE_CHARGE:
        result = battle_target_set_weapon_attack_panels(&action);
        flags = ABILITY_SECONDARY_FLAG_2_NO_FOLLOW_TARGET;
        break;
    case ACTION_MENU_TYPE_JUMP:
        result = battle_target_set_jump_ability_panels(&action);
        flags = ABILITY_SECONDARY_FLAG_2_NO_FOLLOW_TARGET;
        break;
    case ACTION_MENU_TYPE_BLANK_05:
    case ACTION_MENU_TYPE_UNKNOWN_0F:
        return TARGET_PANELS_INVALID;
    default:
        return TARGET_PANELS_NO_TARGETING;
    }
    if (mode == 1) {
        if (ability_id >= ABILITY_ID_ITEM_FIRST) {
            return TARGET_PANELS_INVALID;
        }
        ability = &g_main_ability_range_data[ability_id];
        range = ability->range;
        flags = ability->flags_2;
        if ((ability->flags_1 & ABILITY_SECONDARY_FLAG_1_AUTO) && range == 0
            && !(ability->flags_1 & ABILITY_SECONDARY_FLAG_1_WEAPON_RANGE)) {
            return TARGET_PANELS_NO_TARGETING;
        }
        action.ability_id = ability_id;
        result = battle_target_set_ability_panels(&action);
    }
    if (mode == 2) {
        result = battle_target_spread_ability_range(unit, range);
        flags = ABILITY_SECONDARY_FLAG_2_NO_FOLLOW_TARGET;
    }
    if (result != 0) {
        /* The named temporary is load-bearing; the direct test compiles differently. */
        s32 flag_result = flags & ABILITY_SECONDARY_FLAG_2_NO_FOLLOW_TARGET;

        return flag_result != 0;
    }
    return TARGET_PANELS_NONE;
}

s32 battle_target_calculate_for_menu_types(const action_command_t* source) {
    action_command_t action;
    unit_stats_t* unit;
    u16 ability_id;
    u8 item;
    s32 mode;
    s32 result;

    main_util_copy_action_data(source, &action);
    mode = 1;
    if (action.actor_id >= BATTLE_UNIT_SLOT_COUNT) {
        return -1;
    }
    unit = &g_battle_unit_stats[action.actor_id];
    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
        return -1;
    }
    if (action.targeting_type == BATTLE_TARGETING_TYPE_UNIT && action.target_id >= BATTLE_UNIT_SLOT_COUNT) {
        return -1;
    }
    switch (g_main_action_menu_types_by_skillset[action.skillset_id]) {
    case ACTION_MENU_TYPE_DEFAULT:
    case ACTION_MENU_TYPE_MONSTER:
        ability_id = action.ability_id;
        break;
    case ACTION_MENU_TYPE_ITEM_INVENTORY:
    case ACTION_MENU_TYPE_WEAPON_INVENTORY:
    case ACTION_MENU_TYPE_ATTACK:
    case ACTION_MENU_TYPE_JUMP:
    case ACTION_MENU_TYPE_CHARGE:
        mode = 2;
        break;
    case ACTION_MENU_TYPE_ARITHMETICKS:
        result = battle_target_run_calculator(&action);
        mode = 0;
        break;
    case ACTION_MENU_TYPE_ELEMENTS:
        ability_id = battle_action_get_elemental_ability_id(unit);
        break;
    case ACTION_MENU_TYPE_KATANA_INVENTORY:
        item = action.item_or_equipment;
        if (item < ITEM_ID_ASURA_KNIFE || item > ITEM_ID_CHIRIJIRADEN) {
            return -1;
        }
        ability_id = (u8)action.item_or_equipment + (ABILITY_ID_DRAW_OUT_ASURA - ITEM_ID_ASURA_KNIFE);
        break;
    case ACTION_MENU_TYPE_BLANK_05:
    case ACTION_MENU_TYPE_UNKNOWN_0F:
        return -1;
    default:
        return 2;
    }
    if (mode == 1) {
        if (ability_id >= ABILITY_ID_ITEM_FIRST) {
            return -1;
        }
        action.ability_id = ability_id;
        result = battle_target_set_green_panels_for_action(&action);
    }
    if (mode == 2) {
        result = battle_target_keep_only_target_tile(&action);
    }
    if (result == 0) {
        return 1;
    }
    return -(result == -1);
}

/* Re-validate the acting unit's stored target and mark the target's tile.
 *
 * The weapon's flags select the trajectory test: ARC takes 0x801aff18,
 * DIRECT the direct-weapon check (skipped for an adjacent tile when the
 * skillset menu type is ITEM_INVENTORY), LUNGING 0x8017afc0, and anything else
 * the plain height-overlap test. Attack and Charge skillsets, and abilities
 * flagged 0x20, use the equipped weapon; other skillsets use DIRECT and first
 * copy the target unit's position into the action for targeting type 6.
 * Returns 0 once a valid unit is marked, else -1.
 *
 * One `hand` variable carries both hands, so the right-hand load stays after
 * the left-hand mask; `target` is reused for the result unit, which keeps it a
 * global pseudo and gives the target's v1 for the copied position. */
s32 battle_target_validate_weapon_target(const action_command_t* source) {
    action_command_t action;
    s16 x;
    unit_stats_t* unit;
    unit_stats_t* target;
    s32 flags;
    u8 unit_id;
    u8 menu_type;
    u8 ability_flags;
    s32 hand;
    s32 mask;
    u8 weapon;
    u32 weapon_id;
    map_tile_t* tile;
    s16 y;
    s16 elevation;
    s32 target_id;
    s32 distance_x;
    s32 distance_y;
    s32 distance;
    s32 result;

    main_util_copy_action_data(source, &action);
    unit_id = action.actor_id;
    if (unit_id >= BATTLE_UNIT_SLOT_COUNT) {
        return -1;
    }
    unit = &g_battle_unit_stats[unit_id];
    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
        return -1;
    }
    menu_type = g_main_action_menu_types_by_skillset[action.skillset_id];
    ability_flags = 0;
    if (menu_type == ACTION_MENU_TYPE_DEFAULT || menu_type == ACTION_MENU_TYPE_MONSTER) {
        ability_flags = g_main_ability_range_data[(u8)action.ability_id].flags_1;
    }
    if (menu_type == ACTION_MENU_TYPE_ATTACK || menu_type == ACTION_MENU_TYPE_CHARGE
        || (ability_flags & ABILITY_SECONDARY_FLAG_1_WEAPON_RANGE)) {
        /* An empty left hand contributes 0; an equipped right hand wins. */
        hand = unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON];
        mask = -(hand != ITEM_ID_NONE);
        weapon = hand & mask;
        hand = unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON];
        if (hand != ITEM_ID_NONE) {
            weapon = hand;
        }
        if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FROG)] & STATUS_BYTE_MASK(STATUS_ID_FROG))
            || (unit->unit_flags & UNIT_FLAG_MONSTER) || weapon == ITEM_ID_NONE) {
            weapon = 0;
        }
        weapon_id = weapon;
        if (weapon_id >= ITEM_ID_SHIELD_FIRST) {
            return -1;
        }
        flags = g_main_item_weapon_data[weapon_id].flags;
    } else {
        if (action.targeting_type == BATTLE_TARGETING_TYPE_UNIT) {
            if (action.target_id >= BATTLE_UNIT_SLOT_COUNT) {
                return -1;
            }
            target = &g_battle_unit_stats[action.target_id];
            action.target_position.vx = target->x;
            action.target_position.vz = target->position.y;
            action.target_position.vy = target->position.higher_elevation;
        }
        flags = WEAPON_FLAG_DIRECT;
    }
    x = unit->action_command.target_position.vx;
    y = unit->action_command.target_position.vz;
    elevation = unit->action_command.target_position.vy;
    target_id = battle_target_find_unit_at_tile(x, y, elevation);
    if (target_id < 0) {
        target_id = -1;
    }
    if (flags & WEAPON_FLAG_ARC) {
        result
            = battle_effect_set_and_validate_arc_trajectory(unit_id, &unit->action_command.target_position, target_id);
    } else if (flags & WEAPON_FLAG_DIRECT) {
        distance_x = x - unit->x;
        if (distance_x < 0) {
            distance_x = -distance_x;
        }
        distance_y = y - unit->position.y;
        if (distance_y < 0) {
            distance_y = -distance_y;
        }
        distance = distance_x + distance_y;
        if (menu_type == ACTION_MENU_TYPE_ITEM_INVENTORY && distance < 2) {
            result = target_id;
        } else {
            result = battle_effect_check_direct_trajectory_to_target(
                unit_id, &unit->action_command.target_position, target_id);
        }
    } else if (flags & WEAPON_FLAG_LUNGING) {
        result = battle_target_validate_lunging_target(unit_id, x, y, elevation, target_id);
    } else {
        result = battle_target_validate_height_overlap(unit_id, x, y, elevation, target_id);
    }
    battle_target_disable_green_panel_flags();
    if (result >= 0 && result < BATTLE_UNIT_SLOT_COUNT) {
        target = &g_battle_unit_stats[result];
        tile = &g_battle_map_tile_data[battle_map_calculate_location(target)];
        tile->ceiling_depth_and_marks.targeted = 1;
    }
    return 0;
}

/* Validates a lunging-weapon target one or two tiles away in a straight line.
 *
 * Called by target_validate_weapon_target for WEAPON_FLAG_LUNGING. Adjacent targets
 * pass the plain height-overlap test. At distance 2 the middle tile's two
 * layers (surface height, or an occupant's total height) must leave part of
 * the attacker/target height window open; otherwise a unit on the middle tile
 * whose span contains the attacker's mid-height is returned instead.
 * `elevation` is unused. Returns the unit hit, or -1.
 *
 * `total` doubles as the height swap temporary, and the separate upper_index
 * keeps the target's instruction order for the upper-layer tile address. */
static s32 battle_target_validate_lunging_target(s32 unit_id, s32 x, s32 y, s32 elevation, s32 target_id) {
    battle_unit_height_data_t height;
    unit_stats_t* unit = &g_battle_unit_stats[unit_id];
    map_tile_t* lower;
    map_tile_t* upper;
    s32 unit_x = unit->x;
    s32 unit_y = unit->position.y;
    s32 dx;
    s32 dy;
    s32 distance;
    u8 target_walking;
    u8 target_total;
    u8 unit_walking;
    u8 unit_total;
    s32 mid_x;
    s32 mid_y;
    u8 low;
    u8 high;
    u8 lower_height;
    u8 upper_height;
    s32 lower_id;
    s32 upper_id;
    s32 swap_id;
    u8 total;
    u32 middle;
    s32 layer;
    s32 index;
    s32 upper_index;
    s32 limit;

    if (unit_x < x) {
        dx = x - unit_x;
    } else {
        dx = unit_x - x;
    }
    if (unit_y < y) {
        dy = y - unit_y;
    } else {
        dy = unit_y - y;
    }
    if (dx == 0) {
        if (dy == 1) {
            distance = 1;
        } else if (dy == 2) {
            distance = 2;
        } else {
            return -1;
        }
    } else if (dy != 0) {
        return -1;
    } else if (dx == 1) {
        distance = 1;
    } else if (dx == 2) {
        distance = 2;
    } else {
        return -1;
    }

    battle_unit_calculate_height_data(&height, target_id);
    if (height.unit_flags & BATTLE_UNIT_HEIGHT_UNAVAILABLE) {
        target_id = -1;
    }
    target_total = height.total_height;
    target_walking = height.walking_height;
    battle_unit_calculate_height_data(&height, unit_id);
    if (height.unit_flags & BATTLE_UNIT_HEIGHT_UNAVAILABLE) {
        return -1;
    }
    unit_walking = height.walking_height;
    unit_total = height.total_height;
    if (distance == 1) {
        if (target_id == -1) {
            return -1;
        }
        if (target_walking < unit_total + 2 && target_total + 2 >= unit_walking) {
            return target_id;
        }
        return -1;
    }

    mid_x = (unit_x + x) / 2;
    mid_y = (unit_y + y) / 2;
    if (distance == 2 && target_id != -1 && target_walking < unit_total + 2 && target_total >= unit_walking) {
        low = (unit_walking - 6 < target_walking) ? target_walking : unit_walking - 6;
        limit = unit_total + 2;
        if (target_total < limit) {
            limit = target_total;
        }
        high = limit;
        index = mid_y * g_battle_map_max_x + mid_x;
        lower = &g_battle_map_tile_data[index];
        upper_index = index + 0x100;
        upper = &g_battle_map_tile_data[upper_index];
        lower_height = lower->height * 2 + lower->depth_half_height.half_height;
        upper_height = upper->height * 2 + upper->depth_half_height.half_height;
        lower_id = battle_target_find_unit_at_tile(mid_x, mid_y, 0);
        upper_id = battle_target_find_unit_at_tile(mid_x, mid_y, 1);
        if (lower->flags.blocked) {
            lower_height = 0xff;
        }
        if (upper->flags.blocked) {
            upper_height = 0xff;
        }
        if (lower_height > upper_height) {
            total = upper_height;
            upper_height = lower_height;
            lower_height = total;
            swap_id = upper_id;
            upper_id = lower_id;
            lower_id = swap_id;
        }
        if (lower_height != 0xff) {
            battle_unit_calculate_height_data(&height, lower_id);
            if (!(height.unit_flags & BATTLE_UNIT_HEIGHT_UNAVAILABLE)) {
                lower_height = height.total_height;
            }
            if (low < lower_height) {
                low = lower_height;
            }
            if (high < lower_height) {
                low = 0xff;
            }
        }
        if (upper_height != 0xff) {
            battle_unit_calculate_height_data(&height, upper_id);
            if (!(height.unit_flags & BATTLE_UNIT_HEIGHT_UNAVAILABLE)) {
                total = height.total_height;
                if (low >= upper_height && total >= high) {
                    low = 0xff;
                }
            }
        }
        if (high > low) {
            return target_id;
        }
    }

    middle = (u32)(unit_walking + unit_total) >> 1;
    for (layer = 0; layer < 2; layer++) {
        target_id = battle_target_find_unit_at_tile(mid_x, mid_y, layer);
        battle_unit_calculate_height_data(&height, target_id);
        if (!(height.unit_flags & BATTLE_UNIT_HEIGHT_UNAVAILABLE)) {
            target_walking = height.walking_height;
            target_total = height.total_height;
            if (target_walking < middle && middle < target_total) {
                return target_id;
            }
        }
    }
    return -1;
}

/* Return the target only when its vertical span overlaps the attacker's.
 *
 * The coordinate arguments are unused here but are part of the caller ABI. */
static s32 battle_target_validate_height_overlap(
    s32 attacker_id, s32 target_x, s32 target_y, s32 target_elevation, s32 target_id) {
    battle_unit_height_data_t height_data;
    u32 target_walking_height;
    u32 target_total_height;
    u32 attacker_total_height;
    u32 attacker_walking_height;

    battle_unit_calculate_height_data(&height_data, target_id);
    if ((height_data.unit_flags & BATTLE_UNIT_HEIGHT_UNAVAILABLE) != 0) {
        return -1;
    }
    target_walking_height = height_data.walking_height;
    target_total_height = height_data.total_height;
    battle_unit_calculate_height_data(&height_data, attacker_id);
    if ((height_data.unit_flags & BATTLE_UNIT_HEIGHT_UNAVAILABLE) != 0) {
        return -1;
    }
    attacker_total_height = height_data.total_height;
    attacker_walking_height = height_data.walking_height;
    if (target_walking_height < attacker_total_height && target_total_height >= attacker_walking_height) {
        return target_id;
    }
    return -1;
}

/* Clear MAP_TILE_FLAG_TARGETED on all 512 tiles. */
void battle_target_disable_green_panel_flags(void) {
    map_tile_t* tile;
    s32 i;

    for (i = 0; i < MAP_TILE_COUNT; i++) {
        tile = &g_battle_map_tile_data[i];
        tile->ceiling_depth_and_marks.targeted = 0;
    }
}

/* Marks the tiles of every unit selected by a Calculator (Arithmeticks) action.
 *
 * The type ability picks the compared value (CT, capped at 100 and forced to
 * 100 for the caster, level, EXP, or whole-unit height, where half heights never
 * qualify); the multiplier ability tests it for primality or divisibility by
 * 5, 4 or 3. Units that are absent, crystallized, jumping, treasure, or
 * flagged 0x40 in mount_info are skipped. Returns the number of marked units,
 * or -1 when the command does not name one type and one multiplier ability. */
static s32 battle_target_run_calculator(const action_command_t* source) {
    battle_unit_height_data_t height;
    action_command_t action;
    unit_stats_t* actor;
    unit_stats_t* unit;
    map_tile_t* tile;
    s16 type;
    s16 multiplier;
    u8* type_entry;
    u8* multiplier_entry;
    u8 type_flags;
    u8 multiplier_flags;
    u8 value;
    s32 count = 0;
    s32 hit;
    s32 i;
    s32 j;

    main_util_copy_action_data(source, &action);
    type = action.calculator_type;
    multiplier = action.calculator_multiplier;
    actor = &g_battle_unit_stats[action.actor_id];
    if (type < ABILITY_ID_MATH_FIRST || type >= ABILITY_ID_REACTION_FIRST) {
        return -1;
    }
    if (multiplier < ABILITY_ID_MATH_FIRST || multiplier >= ABILITY_ID_REACTION_FIRST) {
        return -1;
    }
    type_entry = &g_main_math_rsm_ability_data[type - ABILITY_ID_MATH_FIRST];
    multiplier_entry = &g_main_math_rsm_ability_data[multiplier - ABILITY_ID_MATH_FIRST];
    type_flags = *type_entry;
    multiplier_flags = *multiplier_entry;
    if (!(type_flags & CALCULATOR_ATTRIBUTE_MASK) || !(multiplier_flags & CALCULATOR_MULTIPLE_MASK)
        || (type_flags & CALCULATOR_MULTIPLE_MASK) || (multiplier_flags & CALCULATOR_ATTRIBUTE_MASK)) {
        return -1;
    }
    battle_target_disable_green_panel_flags();
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        unit = &g_battle_unit_stats[i];
        if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
            continue;
        }
        if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)]
            & (STATUS_BYTE_MASK(STATUS_ID_CRYSTAL) | STATUS_BYTE_MASK(STATUS_ID_JUMP))) {
            continue;
        }
        if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_TREASURE)] & STATUS_BYTE_MASK(STATUS_ID_TREASURE)) {
            continue;
        }
        if (unit->mount_info & BATTLE_MOUNT_INFO_FLAG_MOUNT) {
            continue;
        }
        if (type_flags & CALCULATOR_ATTRIBUTE_CT) {
            value = unit->ct;
            if (value > UNIT_TURN_CT_THRESHOLD || unit == actor) {
                value = UNIT_TURN_CT_THRESHOLD;
            }
        } else if (type_flags & CALCULATOR_ATTRIBUTE_LEVEL) {
            value = unit->level;
        } else if (type_flags & CALCULATOR_ATTRIBUTE_EXP) {
            value = unit->experience;
        } else {
            battle_unit_calculate_height_data(&height, i);
            value = height.walking_height;
            if (value & 1) {
                continue;
            }
            value >>= 1;
        }
        if (value == 0) {
            continue;
        }
        hit = 0;
        if (multiplier_flags & CALCULATOR_MULTIPLE_PRIME) {
            for (j = 0; j < CALCULATOR_PRIME_COUNT; j++) {
                if (g_main_calculator_primes[j] == value) {
                    hit = 1;
                    break;
                }
            }
        } else if ((multiplier_flags & CALCULATOR_MULTIPLE_FIVE) && value % 5 == 0) {
            hit = 1;
        } else if ((multiplier_flags & CALCULATOR_MULTIPLE_FOUR) && (value & 3) == 0) {
            hit = 1;
        } else if ((multiplier_flags & CALCULATOR_MULTIPLE_THREE) && value % 3 == 0) {
            hit = 1;
        }
        if (hit != 0) {
            tile = &g_battle_map_tile_data[battle_map_calculate_location(unit)];
            count++;
            tile->ceiling_depth_and_marks.targeted = 1;
        }
    }
    return count;
}

/* Leave only the action's target tile marked as targeted. Returns 1, or -1
 * when that tile is off the map or blocked. */
static s32 battle_target_keep_only_target_tile(const action_command_t* action) {
    action_command_t action_copy;
    u8 x;
    u8 y;
    u8 level;
    map_tile_t* tile;

    main_util_copy_action_data(action, &action_copy);
    /* Retail reads only the low byte of each 16-bit coordinate. */
    x = (u8)action_copy.target_position.vx;
    y = (u8)action_copy.target_position.vz;
    level = (u8)action_copy.target_position.vy;
    if (x < g_battle_map_max_x) {
        if (y < g_battle_map_max_y) {
            if (level < 2) {
                tile = &g_battle_map_tile_data[(level << 8) + y * g_battle_map_max_x + x];
                if ((tile->flags.blocked) == 0) {
                    battle_target_disable_green_panel_flags();
                    tile->ceiling_depth_and_marks.targeted = 1;
                    return 1;
                }
            }
        }
    }
    return -1;
}

/*
 * Mark the panels an AI action would affect and return how many are
 * targetable.
 *
 * `location` carries the column into the index sum: the extra copies give the
 * index pseudo the allocation priority the target needs (a2), while keeping the
 * `(elevation * 256 + y * width) + x` operand order.
 */
static s32 battle_target_set_green_panels_for_action(const action_command_t* action) {
    /* A byte collision test retains the original single-mask branch. */
    action_command_t action_copy;
    ability_secondary_data_t* ability;
    unit_stats_t* unit;
    unit_stats_t* target;
    map_tile_t* tile;
    /* Byte access retains the original packed load and field-extraction order. */
    target_panel_t* panel;
    target_panel_t* other_panel;
    target_panel_t* cursor;
    target_panel_t* marked;
    s32 x;
    s32 y;
    s32 elevation;
    s32 index;
    s32 location;
    s32 height;
    s32 i;
    s32 count;
    u8 unit_id;
    u8 flags_1;
    u8 flags_2;
    u8 aoe;
    u8 vertical;

    main_util_copy_action_data(action, &action_copy);
    ability = &g_main_ability_range_data[action_copy.ability_id];
    flags_1 = ability->flags_1;
    flags_2 = ability->flags_2;
    aoe = ability->aoe;
    vertical = ability->vertical;
    unit_id = action_copy.actor_id;
    unit = &g_battle_unit_stats[unit_id];
    if (ability->flags_4 & ABILITY_SECONDARY_FLAG_4_DIRECT_TARGETING) {
        flags_1 |= ABILITY_SECONDARY_FLAG_1_WEAPON_RANGE;
        flags_2 &= ABILITY_SECONDARY_FLAG_2_CANNOT_HIT_CASTER | ABILITY_SECONDARY_FLAG_2_RANDOM_FIRE
            | ABILITY_SECONDARY_FLAG_2_NO_FOLLOW_TARGET | ABILITY_SECONDARY_FLAG_2_TOP_DOWN;
    }
    if (flags_2 & (ABILITY_SECONDARY_FLAG_2_THREE_DIRECTIONS | ABILITY_SECONDARY_FLAG_2_LINEAR_ATTACK)) {
        x = unit->x;
        y = unit->position.y;
        elevation = unit->position.higher_elevation;
    } else if (action_copy.targeting_type != BATTLE_TARGETING_TYPE_UNIT) {
        x = (u8)action_copy.target_position.vx;
        y = (u8)action_copy.target_position.vz;
        elevation = (u8)action_copy.target_position.vy;
    } else {
        target = &g_battle_unit_stats[action_copy.target_id];
        x = target->x;
        y = target->position.y;
        elevation = target->position.higher_elevation;
    }
    if (x >= g_battle_map_max_x) {
        return -1;
    }
    if (y >= g_battle_map_max_y || elevation >= 2) {
        return -1;
    }
    location = x;
    index = location;
    index = elevation * 256 + y * g_battle_map_max_x + location;
    tile = &g_battle_map_tile_data[index];
    if (tile->flags.blocked) {
        return -1;
    }
    height = tile->height * 2 + ((*(u8*)&tile->depth_half_height) & MAP_TILE_HALF_HEIGHT_MASK)
        + ((*(u8*)&tile->depth_half_height) >> 5) * 2;
    if (aoe == 0xff) {
        cursor = g_battle_target_panels;
        for (i = 0; i < 0x200; i++) {
            cursor[i].remaining_range = 1;
            cursor[i].mark = 0;
        }
    } else {
        cursor = g_battle_target_panels;
        for (i = 0; i < 0x200; i++) {
            cursor[i].remaining_range = 0;
            cursor[i].mark = 0;
        }
        panel = &g_battle_target_panels[index];
        panel->remaining_range = aoe + 1;
        if (index < 0x100) {
            other_panel = &g_battle_target_panels[index + 0x100];
            panel->mark = 1;
        } else {
            other_panel = &g_battle_target_panels[index - 0x100];
            other_panel->mark = 1;
        }
        g_battle_target_panels[index & 0xff].mark = 1;
        if (!(flags_1 & ABILITY_SECONDARY_FLAG_1_WEAPON_RANGE)) {
            panel->remaining_range = aoe + 1;
            other_panel->remaining_range = aoe + 1;
            battle_target_spread_panels(aoe, 0);
            battle_target_apply_vertical_tolerance(
                height, vertical, (flags_2 & ABILITY_SECONDARY_FLAG_2_TOP_DOWN) != 0);
            battle_target_check_moldball_virus_depth(action_copy.ability_id);
        }
    }
    cursor = &g_battle_target_panels[battle_map_calculate_location(unit)];
    if (flags_2 & ABILITY_SECONDARY_FLAG_2_CANNOT_HIT_CASTER) {
        cursor->remaining_range = 0;
    }
    if (flags_2 & ABILITY_SECONDARY_FLAG_2_LINEAR_ATTACK) {
        battle_target_build_directional_attack_panels(&action_copy, 1);
    }
    if (flags_2 & ABILITY_SECONDARY_FLAG_2_THREE_DIRECTIONS) {
        battle_target_build_directional_attack_panels(&action_copy, 3);
    }
    if (flags_2 & (ABILITY_SECONDARY_FLAG_2_CAN_TARGET_ENEMIES | ABILITY_SECONDARY_FLAG_2_CAN_TARGET_ALLIES)) {
        battle_target_apply_unit_team_eligibility(unit_id, flags_2 & ABILITY_SECONDARY_FLAG_2_CAN_TARGET_ALLIES,
            flags_2 & ABILITY_SECONDARY_FLAG_2_CAN_TARGET_ENEMIES, aoe == 0xff);
    }
    count = 0;
    if (!(flags_2 & (ABILITY_SECONDARY_FLAG_2_CAN_TARGET_ENEMIES | ABILITY_SECONDARY_FLAG_2_CAN_TARGET_ALLIES))) {
        tile = g_battle_map_tile_data;
        for (i = 0, marked = g_battle_target_panels; i < 0x200; i++) {
            if (marked->remaining_range != 0 && !(((*(u8*)&tile[i].flags) & MAP_TILE_COLLISION_MASK))) {
                count++;
                tile[i].ceiling_depth_and_marks.targeted = 1;
            } else {
                tile[i].ceiling_depth_and_marks.targeted = 0;
            }
            marked++;
        }
        return count;
    }
    tile = g_battle_map_tile_data;
    for (i = 0, marked = g_battle_target_panels; i < 0x200; i++) {
        if (marked->mark != 0 && !(((*(u8*)&tile[i].flags) & MAP_TILE_COLLISION_MASK))) {
            count++;
            tile[i].ceiling_depth_and_marks.targeted = 1;
        } else {
            tile[i].ceiling_depth_and_marks.targeted = 0;
        }
        marked++;
    }
    return count;
}

/* Filters unit-occupied targeting panels by team eligibility.
 *
 * The low byte of `raw_unit_id` selects the acting unit, which is treated as
 * same-team even if its initial and live team fields differ. `allow_allies`
 * and `allow_enemies` are raw nonzero flag values. Unless the ability AoE is
 * `0xff`, every nonempty panel left after filtering is marked selected. */
static void battle_target_apply_unit_team_eligibility(
    s32 raw_unit_id, u8 allow_allies, u8 allow_enemies, u8 aoe_is_0xff) {
    s32 relation;
    u8 team_flags;
    s32 panel_index;
    target_panel_t* panel;
    s32 i;
    s32 actor_id;

    raw_unit_id &= 0xff;
    team_flags = g_battle_unit_stats[raw_unit_id].team_flags;
    actor_id = raw_unit_id;
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        if (battle_target_is_unit_untargetable_and_store_tile(i, &panel_index) == 0) {
            relation = team_flags ^ g_battle_unit_stats[i].initial_team_flags;
            if (actor_id == i) {
                relation = 0;
            }
            panel = &g_battle_target_panels[panel_index];
            if (panel->remaining_range != 0) {
                if (relation & BATTLE_TEAM_MASK) {
                    if (allow_enemies == 0) {
                        panel->remaining_range = 0;
                    } else {
                        panel->mark = 1;
                    }
                } else if (allow_allies != 0) {
                    panel->mark = 1;
                } else {
                    panel->remaining_range = 0;
                }
            }
        }
    }
    if (!aoe_is_0xff) {
        for (i = 0; i < MAP_TILE_COUNT; i++) {
            panel = &g_battle_target_panels[i];
            if (panel->remaining_range != 0) {
                panel->mark = 1;
            }
        }
    }
}

/* Store a unit's tile and report whether targeting must reject the unit. */
s32 battle_target_is_unit_untargetable_and_store_tile(s32 unit_id, s32* tile_index) {
    unit_stats_t* unit = &g_battle_unit_stats[unit_id];

    *tile_index = battle_map_calculate_location(unit);
    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
        return 1;
    } else if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)]
        & (STATUS_BYTE_MASK(STATUS_ID_CRYSTAL) | STATUS_BYTE_MASK(STATUS_ID_JUMP))) {
        return 1;
    } else if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_TREASURE)]
        & STATUS_BYTE_MASK(STATUS_ID_TREASURE)) {
        return 1;
    } else if (unit->mount_info & BATTLE_MOUNT_INFO_FLAG_MOUNT) {
        return 1;
    }
    return 0;
}

/* Narrow an ability's panels to the straight lines that leave the acting unit
 * toward its target: mode 1 for a linear attack, mode 3 for a three-direction
 * attack.
 *
 * On each axis where the target tile differs from the unit's, the line in that
 * direction is kept; mode 3 also keeps the two perpendicular lines. Every panel
 * off the kept lines loses its range, and the marks that identified the lines are
 * cleared. The call also turns on nearest-first ordering of the target list
 * (g_battle_sort_targets_nearest_first). */
static void battle_target_build_directional_attack_panels(const action_command_t* source, u8 mode) {
    action_command_t action;
    unit_stats_t* unit;
    target_panel_t* panel;
    s32 target_x;
    s32 target_y;
    s32 x;
    s32 y;
    s32 i;

    main_util_copy_action_data(source, &action);
    g_battle_sort_targets_nearest_first = 1;
    unit = &g_battle_unit_stats[action.actor_id];
    x = unit->x;
    y = unit->position.y;
    target_x = action.target_position.vx;
    target_y = action.target_position.vz;
    if (x < target_x) {
        battle_target_calculate_linear_attack_tiles(TARGET_LINE_EAST, x, y);
        if (mode == 3) {
            battle_target_calculate_linear_attack_tiles(TARGET_LINE_NORTH, x, y);
            battle_target_calculate_linear_attack_tiles(TARGET_LINE_SOUTH, x, y);
        }
    }
    if (target_x < x) {
        battle_target_calculate_linear_attack_tiles(TARGET_LINE_WEST, x, y);
        if (mode == 3) {
            battle_target_calculate_linear_attack_tiles(TARGET_LINE_NORTH, x, y);
            battle_target_calculate_linear_attack_tiles(TARGET_LINE_SOUTH, x, y);
        }
    }
    if (y < target_y) {
        battle_target_calculate_linear_attack_tiles(TARGET_LINE_NORTH, x, y);
        if (mode == 3) {
            battle_target_calculate_linear_attack_tiles(TARGET_LINE_EAST, x, y);
            battle_target_calculate_linear_attack_tiles(TARGET_LINE_WEST, x, y);
        }
    }
    if (target_y < y) {
        battle_target_calculate_linear_attack_tiles(TARGET_LINE_SOUTH, x, y);
        if (mode == 3) {
            battle_target_calculate_linear_attack_tiles(TARGET_LINE_WEST, x, y);
            battle_target_calculate_linear_attack_tiles(TARGET_LINE_EAST, x, y);
        }
    }
    for (i = 0; i < MAP_TILE_COUNT; i++) {
        panel = &g_battle_target_panels[i];
        if (panel->mark == 0) {
            panel->remaining_range = 0;
        } else {
            panel->mark = 0;
        }
    }
}

/* Mark the panels along a straight line from (x, y) in one compass direction
 * (TARGET_LINE_*), the starting tile included.
 *
 * On both map levels a target_panel_t with remaining_range gets mark set;
 * the table is g_battle_target_panels. The walk stops
 * at the map edge or after 32 tiles, and any other direction value marks nothing.
 * `dir` is reused as the panel index. */
static void battle_target_calculate_linear_attack_tiles(s32 dir, s32 x, s32 y) {
    s32 i;
    s32 dx;
    s32 dy;
    s32 map_width;
    target_panel_t* panel;

    switch (dir) {
    case TARGET_LINE_EAST:
        dx = 1;
        dy = 0;
        break;
    case TARGET_LINE_WEST:
        dx = -1;
        dy = 0;
        break;
    case TARGET_LINE_NORTH:
        dx = 0;
        dy = 1;
        break;
    case TARGET_LINE_SOUTH:
        dx = 0;
        dy = -1;
        break;
    default:
        return;
    }

    i = 0;
    do {
        if (x < 0) {
            return;
        }
        map_width = g_battle_map_max_x;
        if (x >= map_width) {
            return;
        }
        if (y < 0) {
            return;
        }
        if (y >= g_battle_map_max_y) {
            return;
        }
        dir = y * map_width + x;
        panel = &g_battle_target_panel_data[dir];
        if (panel->remaining_range != 0) {
            panel->mark = 1;
        }
        dir += 0x100;
        panel = &g_battle_target_panel_data[dir];
        y += dy;
        if (panel->remaining_range != 0) {
            panel->mark = 1;
        }
        i += 1;
        x += dx;
    } while (i < 0x20);
}
