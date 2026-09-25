#include "fft/battle.h"
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
 * constants and `g_main_terrain_movement_cost_tables[move_mod]` let sched1 reorder the pre-loop
 * `li s2`/`li a3`/`lbu` sequence.
 */
void battle_move_store_unit_movement_to_scratchpad(s32 unit_id) {
    battle_move_pathfind_scratch_t* config;
    battle_move_record_t* record;
    battle_move_record_t* current;
    battle_stats_t* unit;
    battle_stats_t* other;
    map_tile_t* tile;
    u8 movement[3];
    battle_unit_height_data_t height_data;
    s32 i;
    s32 count;
    s32 weather;
    s32 ignore_terrain;
    u8* table;
    u32 value;
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
    battle_calculate_unit_height_data(&height_data, unit_id);
    unit = &g_battle_unit_stats[unit_id];
    for (i = 0; i < 3; i++) {
        movement[i] = unit->movement_abilities[i];
    }
    config->unit_id = unit_id;
    config->x = unit->x;
    config->y = unit->position.bits.y;
    config->high_elevation = unit->position.bits.higher_elevation;
    config->map_max_x = g_battle_map_max_x;
    config->map_max_y = g_battle_map_max_y;
    config->cannot_enter_water = 0;
    config->cannot_stay_on_water = 0;
    config->will_drown = 1;
    config->will_sink = 1;
    config->fly_or_teleport = 0;
    config->stepping_stone = unit->position.bits.stepping_stone;
    if (movement[2] & 0x28) {
        config->can_pass_lava = 1;
    } else {
        config->can_pass_lava = 0;
    }
    value = unit->status_sets.current[2];
    if (value & 6) {
        movement[1] &= 0xF3;
        movement[2] &= 0xFB;
        config->unit_size = 4;
    } else {
        config->unit_size = 6;
    }
    if (!(movement[2] & 0x88) && (movement[1] & 0x10)) {
        config->cannot_stay_on_water = 1;
    }
    if (movement[2] & 8) {
        config->unit_size += 2;
        config->movement_set_3 = movement[2] | 0x80;
    } else {
        config->movement_set_3 = movement[2];
    }
    if (value & 0x40) {
        movement[2] |= 8;
    }
    if (movement[1] & 0x10) {
        movement[2] &= 0xAF;
    }
    weather = battle_map_get_weather_severity();
    if (movement[2] & 4) {
        config->move_mod = 1;
        config->move_type = 0;
        config->fly_or_teleport = 1;
    } else if (movement[1] & 0xC) {
        config->move_mod = 1;
        config->movement_set_2 = movement[1];
        config->move_type = 1;
        config->fly_or_teleport = 1;
    } else {
        if (movement[2] & 8) {
            config->move_mod = 2;
            config->move_type = BATTLE_MOVEMENT_CLASS_FLOAT;
        } else if (movement[2] & 0x20) {
            config->move_mod = 5;
            config->move_type = BATTLE_MOVEMENT_CLASS_LAVA;
        } else if (movement[2] & 0x80) {
            config->move_mod = 3;
            config->move_type = BATTLE_MOVEMENT_CLASS_WATER_SURFACE;
        } else if (movement[2] & 0x40) {
            config->move_mod = 3;
            config->move_type = BATTLE_MOVEMENT_CLASS_WATER_DEPTH_ONE;
        } else if (movement[2] & 0x10) {
            config->move_mod = 4;
            config->move_type = BATTLE_MOVEMENT_CLASS_UNDERWATER;
        } else {
            config->move_mod = 0;
            config->move_type = BATTLE_MOVEMENT_CLASS_NORMAL;
        }
    }
    if (config->movement_set_3 & 0xC0) {
        config->will_sink = 0;
    }
    if (config->movement_set_3 & 0xD0) {
        config->will_drown = 0;
    }
    if (!(movement[2] & 0x8C) && (movement[1] & 0x10)) {
        config->cannot_enter_water = 1;
    }
    config->movement_3 = movement[2];
    if (movement[1] & 2) {
        weather = 1;
    }
    ignore_terrain = movement[1] & 1;
    i = 0;
    blocked = 0xFF;
    table = g_main_terrain_movement_cost_tables[0];
    table += config->move_mod * 64;
    for (; i < 0x40; i++) {
        byte = table[i];
        if (byte == 0) {
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
    config->jump_half = (u8)value >> 1;
    if (config->move_type == 0 || (movement[0] & 2)) {
        value = 0x1F;
    }
    config->jump_times_two = (value & 0xFF) * 2;
    config->jump_or_1f = value;
    if (battle_unit_check_chocobo(unit) != 0) {
        config->mountable_chocobo = 1;
    } else {
        config->mountable_chocobo = 0;
    }
    value = unit->move;
    if (value >= 0x7D) {
        value = 0x7C;
    }
    config->move = value;
    if ((unit->unit_flags & 0xC0) && !(unit->status_sets.current[2] & 6) && !(unit->status_sets.current[4] & 0x20)) {
        config->can_ride = 1;
    } else {
        config->can_ride = 0;
    }
    team = unit->initial_team_flags;
    config->field_1a = D_8018F4FC;
    mount = 0xFF;
    for (i = 15; i >= 0; i--) {
        record[i].unit_id_flags = mount;
    }
    count = 0;
    for (i = 0; i < 21; i++) {
        if (count >= 16) {
            break;
        }
        other = &g_battle_unit_stats[i];
        if (other->entd_slot == BATTLE_ENTD_SLOT_NONE) {
            continue;
        }
        if (other->status_sets.current[0] & 0x64) {
            continue;
        }
        if (other->status_sets.current[1] & 1) {
            continue;
        }
        if (i == unit_id) {
            continue;
        }
        mount = other->mount_info;
        if ((mount & 0x80) && (mount & 0x1F) == config->unit_id) {
            config->unit_size += 2;
            continue;
        }
        value = i;
        if (battle_unit_check_chocobo(other) != 1 && other->mount_info != (config->unit_id | 0x40)) {
            value = i | 0x20;
        }
        byte = other->team_flags;
        if ((team ^ byte) & 0x30) {
            value |= 0x40;
        }
        current = &record[count];
        current->unit_id_flags = value;
        current->x = other->x;
        current->y = other->position.bits.y;
        current->higher_elevation = other->position.bits.higher_elevation;
        current->stepping_stone = other->position.bits.stepping_stone;
        value = other->status_sets.current[2];
        if (value & 6) {
            current->body_height = 4;
            current->stepping_stone = 0;
        } else {
            current->body_height = 6;
        }
        value = other->mount_info;
        if ((value & 0x40) && (value & 0x1F) != config->unit_id) {
            current->body_height += 2;
        }
        movement_3 = other->movement_abilities[2];
        tile = &g_battle_map_tile_data[battle_map_calculate_location(other)];
        base = tile->height * 2 + (tile->depth_half_height & 0x1F);
        depth = tile->depth_half_height >> 5;
        byte = base;
        if (depth != 0) {
            if (other->status_sets.current[2] & 0x40) {
                adjust = depth * 2 + 2;
                byte = base + adjust;
            } else if (movement_3 & 0x80) {
                adjust = depth * 2;
                byte = base + adjust;
            } else if (movement_3 & 0x40) {
                adjust = depth * 2 - 2;
                byte = base + adjust;
            }
        } else if (other->status_sets.current[2] & 0x40) {
            byte = base + 2;
        }
        current->standing_height = byte;
        current->top_height = byte + current->body_height;
        count++;
    }
}
