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
