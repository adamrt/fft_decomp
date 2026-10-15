/* ENTD loading, deployment placement and slot existence changes. */

#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_script.h"
#include "fft/battle_unit.h"
#include "fft/script.h"
#include "psx/types.h"

/* Initialize one battle slot from the current party entry.
 *
 * Only unit_index is observed; no direct caller or callback table is proven. */
s32 battle_unit_init_party_member_battle_data(s32 unused0, s32 unused1, s32 unit_index) {
    unit_stats_t* unit;
    s32 formation_id;

    unit = &g_battle_unit_stats[unit_index];
    formation_id = g_main_unit_deployed_coords->party_id;
    unit->battle_id = unit_index;
    if (main_unit_init_job_data(unit, formation_id, 0) != 0) {
        return -1;
    }
    main_unit_init_status_and_rewards(unit, 1);
    return 0;
}

/* Initialize deployed units from map coordinates and return a failure bitmask.
 *
 * Normal deployment fills slots 0x10..0x14; red team debug deployment fills
 * slots 0..4 and clears the remaining ENTD slots. The target reloads each row
 * from g_main_unit_deployed_coords at its distinct uses.
 */
s32 battle_unit_init_deployed_units_data(unit_deployment_coordinates_t* formation, s32 is_red_team_debug) {
    s32 failures = 0;
    s32 weight = 1;
    s32 i;
    s32 slot_step;
    s32 slot_start;
    s32 next_generated_unit_id;
    u8 palette;

    g_main_unit_deployed_coords = formation;
    if (is_red_team_debug) {
        next_generated_unit_id = 0xfe;
        slot_step = -1;
        slot_start = 0;
    } else {
        next_generated_unit_id = 0x78;
        slot_step = 1;
        slot_start = BATTLE_DEPLOYED_UNIT_SLOT_FIRST;
    }
    for (i = 0; i < 5; i++, weight *= 2) {
        s32 battle_id = i + slot_start;
        unit_stats_t* unit = &g_battle_unit_stats[battle_id];
        u8 sprite;
        u32 identity;

        unit->battle_id = battle_id;
        if (g_main_unit_deployed_coords[i].party_id != PARTY_ID_NONE) {
            unit->entd_slot = battle_id;
            unit->existence = BATTLE_UNIT_EXISTENCE_ACTIVE;
            if (main_unit_init_job_data(unit, g_main_unit_deployed_coords[i].party_id, 0) != 0) {
                main_system_handle_battle_load_exception(battle_id + 0x1f4);
            } else if (battle_unit_set_placement_and_validate(battle_id, &g_main_unit_deployed_coords[i]) != 0) {
                main_system_handle_battle_load_exception(battle_id + 0x190);
            } else {
                sprite = main_unit_calculate_palette_spritesheet(unit, &palette);
                identity = unit->character_identity;
                if ((identity & 0xff) == 0 || (identity & 0xff) >= 4) {
                    identity = next_generated_unit_id;
                    next_generated_unit_id += slot_step;
                }
                unit->unit_id = identity;
                if (is_red_team_debug) {
                    unit->team_flags = BATTLE_TEAM_RED | BATTLE_TEAM_FLAG_PLAYER_CONTROLLED;
                    unit->initial_team_flags = BATTLE_TEAM_RED | BATTLE_TEAM_FLAG_PLAYER_CONTROLLED;
                    unit->sprite_palette = 1;
                }
                if (g_main_unit_deployed_coords[i].flags.off_field) {
                    unit->existence = BATTLE_UNIT_EXISTENCE_OFF_FIELD;
                    unit->entd_slot = BATTLE_ENTD_SLOT_NONE;
                } else {
                    battle_gfx_append_unit_graphics_load_descriptor(unit->x, unit->position.y,
                        unit->position.higher_elevation, unit->position.facing, sprite, palette, 0xff, unit, 0);
                    main_unit_init_status_and_rewards(unit, 1);
                    unit->existence = BATTLE_UNIT_EXISTENCE_ACTIVE;
                }
                continue;
            }
            failures += weight;
        }
        unit->entd_slot = BATTLE_ENTD_SLOT_NONE;
        unit->existence = BATTLE_UNIT_EXISTENCE_NONE;
    }
    if (is_red_team_debug) {
        for (i = 5; i < ENTD_UNITS_PER_ENCOUNTER; i++) {
            unit_stats_t* unit = &g_battle_unit_stats[i];
            unit->battle_id = i;
            unit->entd_slot = BATTLE_ENTD_SLOT_NONE;
            unit->existence = BATTLE_UNIT_EXISTENCE_NONE;
        }
    }
    return failures;
}

/* Clears g_battle_enemy_level_sum, then runs battle_unit_init_deployed_units_data
 * as the red team's debug deployment. The second argument is ignored. */
void battle_unit_init_deployed_units_data_for_debug_red_team(unit_deployment_coordinates_t* formation, s32 unused_1) {
    g_battle_enemy_level_sum = 0;
    battle_unit_init_deployed_units_data(formation, 1);
}

/* battle_unit_init_deployed_units_data for a normal deployment. The second
 * argument is ignored. */
s32 battle_unit_build_deployed_units_data(unit_deployment_coordinates_t* formation, s32 unused_1) {
    return battle_unit_init_deployed_units_data(formation, 0);
}

/* Store the deployment position and return its placement validation result.
 *
 * The caller at 0x8017f48c tests the returned value after this function's
 * final call to battle_unit_validate_placement. */
s32 battle_unit_set_placement_and_validate(s32 unit_idx, unit_deployment_coordinates_t* src) {
    unit_stats_t* unit = &g_battle_unit_stats[unit_idx];

    unit->position.higher_elevation = src->flags.upper_level;
    unit->position.y = src->y;
    unit->x = src->x;
    unit->position.facing = src->flags.facing;
    return battle_unit_validate_placement(unit_idx);
}

/* Validate a unit's placement tile.
 *
 * Returns -1 when the position is off the map, the tile is unwalkable, too
 * steep or deep for the unit's movement abilities, lava or an obstacle, or
 * already occupied by an earlier unit slot at the same elevation; 0 otherwise. */
s32 battle_unit_validate_placement(s32 unit_idx) {
    /* A byte collision test retains the original single-mask branch. */
    unit_stats_t* unit = &g_battle_unit_stats[unit_idx];
    unit_stats_t* other;
    map_tile_t* tile;
    u8 x;
    u8 y;
    u8 higher_elevation;
    s32 i;

    x = unit->x;
    y = unit->position.y;
    higher_elevation = unit->position.higher_elevation;
    if (x >= g_map_max_x) {
        return -1;
    }
    if (y >= g_map_max_y) {
        return -1;
    }
    tile = &g_map_tile_data[map_calculate_location(unit)];
    if (((*(u8*)&tile->flags) & MAP_TILE_COLLISION_MASK)) {
        return -1;
    }
    if (tile->depth_half_height.half_height >= 3) {
        return -1;
    }
    if ((unit->ability_flags.movement[1] & BATTLE_MOVEMENT_SET_2_CANNOT_ENTER_WATER)
        && tile->depth_half_height.depth != 0 && !(unit->ability_flags.movement[2] & BATTLE_MOVEMENT_SET_3_FLOAT)) {
        return -1;
    }
    if (tile->depth_half_height.depth >= 3
        && !(unit->ability_flags.movement[2]
            & (BATTLE_MOVEMENT_SET_3_MOVE_IN_WATER | BATTLE_MOVEMENT_SET_3_WALK_ON_WATER
                | BATTLE_MOVEMENT_SET_3_MOVE_UNDERWATER | BATTLE_MOVEMENT_SET_3_FLOAT))) {
        return -1;
    }
    if (tile->surface.type == MAP_SURFACE_LAVA || tile->surface.type == MAP_SURFACE_OBSTACLE) {
        return -1;
    }
    for (i = 0; i < unit_idx; i++) {
        other = &g_battle_unit_stats[i];
        if (other->entd_slot != BATTLE_ENTD_SLOT_NONE && other->x == x && other->position.y == y
            && other->position.higher_elevation == higher_elevation) {
            return -1;
        }
    }
    return 0;
}

/* Loads the 16 ENTD units of the encounter into the battle unit slots.
 *
 * Each present unit is initialized. A unit that would need a new spritesheet
 * beyond the sheets left for the ENTD (nine less the deployment unit limit) is
 * taken off the field, and the units placed on the field are capped at that
 * number + 7; the exception reports use the error bases 600, 300, 100 and 200
 * plus the slot. `status` holds both the initialization result and the
 * graphics descriptor's not-deployed flag: sharing the one variable is what
 * gives the flag enough uses to win s1 ahead of battle_id. */
void battle_unit_load_entd_units(entd_encounter_t* entd_data, s32 mode) {
    s32 battle_id;
    s32 enemy_count;
    s32 free_spritesheets;
    s32 enemy_limit;
    s32 duplicate_message_shown = 0;
    s32 limit_message_shown = 0;
    s32 status;
    u8 next_unused_unit_id = 0xfe;
    u8 palette_id;
    u8 spritesheet_id;
    s32 unit_id;
    u8 presence;
    unit_stats_t* unit;
    entd_unit_t* entd;
    entd_unit_t* entd_list = entd_data->units;

    g_battle_player_unit_fallen = 0;
    g_battle_enemy_level_sum = 0;
    g_battle_highest_enemy_level = 0;
    main_party_calculate_highest_level();
    free_spritesheets = script_get_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_UNIT_LIMIT);
    free_spritesheets = (free_spritesheets < 10) ? 9 - free_spritesheets : 0;
    enemy_limit = free_spritesheets + 7;
    enemy_count = 0;
    battle_unit_add_unique_entd_spritesheet(0xff, 0, 0);
    for (battle_id = 0, entd = entd_list; battle_id < ENTD_UNITS_PER_ENCOUNTER; entd++, battle_id++) {
        unit = &g_battle_unit_stats[battle_id];
        unit->battle_id = battle_id;
        g_main_entd_current_unit = entd;
        if (entd->sprite_set == CHARACTER_IDENTITY_ENTD_NONE) {
            unit->entd_slot = BATTLE_ENTD_SLOT_NONE;
            unit->existence = BATTLE_UNIT_EXISTENCE_NONE;
            continue;
        }

        unit->entd_slot = battle_id;
        unit->existence = BATTLE_UNIT_EXISTENCE_ACTIVE;
        status = main_unit_init_for_battle(unit, g_main_entd_current_unit, 0, 0);
        if (status != 0) {
            main_system_handle_battle_load_exception(battle_id + 600);
        }
        spritesheet_id = main_unit_calculate_palette_spritesheet(unit, &palette_id);
        main_unit_init_status_and_rewards(unit, 0);
        /* s32 so the fallback id is zero-extended once here and passed on
         * unmasked, like the target. */
        unit_id = g_main_entd_current_unit->unit_id;
        if (unit_id == 0xff) {
            unit_id = next_unused_unit_id--;
        }
        unit->unit_id = unit_id;

        presence = unit->team_flags & BATTLE_TEAM_PRESENCE_MASK;
        if (presence == 0 || (presence == BATTLE_TEAM_FLAG_RANDOMLY_PRESENT && (rand() & 1))) {
            unit->entd_slot = BATTLE_ENTD_SLOT_NONE;
            unit->existence = BATTLE_UNIT_EXISTENCE_OFF_FIELD;
            main_unit_init_position_and_rewards(unit, g_main_entd_current_unit);
            continue;
        }

        main_unit_init_position_and_rewards(unit, g_main_entd_current_unit);
        if (battle_unit_validate_placement(battle_id) != 0) {
            main_system_handle_battle_load_exception(battle_id + 300);
            unit->entd_slot = BATTLE_ENTD_SLOT_NONE;
            unit->existence = BATTLE_UNIT_EXISTENCE_NONE;
            continue;
        }
        if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
            continue;
        }
        if (battle_unit_add_unique_entd_spritesheet(battle_id, spritesheet_id, free_spritesheets) != 0) {
            unit->entd_slot = BATTLE_ENTD_SLOT_NONE;
            unit->existence = BATTLE_UNIT_EXISTENCE_OFF_FIELD;
            if (duplicate_message_shown != 0) {
                continue;
            }
            main_system_handle_battle_load_exception(battle_id + 100);
            duplicate_message_shown = 1;
            continue;
        }

        if (presence != BATTLE_TEAM_PRESENCE_MASK && enemy_count >= enemy_limit) {
            unit->entd_slot = BATTLE_ENTD_SLOT_NONE;
            unit->existence = BATTLE_UNIT_EXISTENCE_NONE;
            if (limit_message_shown != 0) {
                continue;
            }
            main_system_handle_battle_load_exception(battle_id + 200);
            limit_message_shown = 1;
            enemy_count++;
            continue;
        }
        if (presence == BATTLE_TEAM_PRESENCE_MASK) {
            unit->entd_slot = BATTLE_ENTD_SLOT_NONE;
            unit->existence = BATTLE_UNIT_EXISTENCE_OFF_FIELD;
            status = 1;
        } else {
            status = 0;
            battle_unit_set_enemy_level_data(unit);
        }
        battle_gfx_append_unit_graphics_load_descriptor(unit->x, unit->position.y, unit->position.higher_elevation,
            unit->position.facing, spritesheet_id, palette_id, unit_id, unit, status);
        if (presence != BATTLE_TEAM_PRESENCE_MASK) {
            enemy_count++;
        }
    }
}

/* Keeps the list of distinct spritesheets the ENTD units need.
 *
 * Mode 0xff empties the list and mode 0 restarts it with value; any other mode
 * adds value when it is not listed yet. Returns 1 when value's position in the
 * list is limit or more (no spritesheet is left for it), else 0. */
s32 battle_unit_add_unique_entd_spritesheet(s32 mode, s16 value, s32 limit) {
    s32 idx;
    s32 i;

    if (mode == 0xFF) {
        g_battle_unit_entd_spritesheet_count = 0;
        return 0;
    }
    if (mode == 0) {
        g_battle_unit_entd_spritesheet_count = 1;
        g_battle_unit_entd_spritesheet_ids[0] = value;
        return 0;
    }
    idx = 0xFF;
    for (i = 0; i < g_battle_unit_entd_spritesheet_count; i++) {
        if (g_battle_unit_entd_spritesheet_ids[i] == value) {
            idx = i;
            break;
        }
    }
    if (idx == 0xFF) {
        idx = g_battle_unit_entd_spritesheet_count;
        g_battle_unit_entd_spritesheet_count = idx + 1;
        g_battle_unit_entd_spritesheet_ids[idx] = value;
    }
    return idx >= limit;
}

/* Takes a present unit out of the battle: the slot is emptied and its existence
 * set to BATTLE_UNIT_EXISTENCE_DISABLED. Always returns 0. */
s32 battle_unit_disable_remove(s32 battle_id) {
    unit_stats_t* unit = &g_battle_unit_stats[battle_id];
    if (unit->entd_slot != BATTLE_ENTD_SLOT_NONE) {
        unit->entd_slot = BATTLE_ENTD_SLOT_NONE;
        unit->existence = BATTLE_UNIT_EXISTENCE_DISABLED;
    }
    return 0;
}

/* Puts an off-field unit on the field: the slot becomes occupied, its existence
 * active, and a unit of any team but blue is counted into the enemy level
 * statistics.
 *
 * Returns 0, also for a unit already present, or -1 when the slot is empty in any
 * state other than off-field. */
s32 battle_unit_set_enemy_level_data_by_battle_id(u32 battle_id) {
    unit_stats_t* stats = &g_battle_unit_stats[battle_id];

    if (stats->entd_slot != BATTLE_ENTD_SLOT_NONE)
        return 0;
    if (stats->existence != BATTLE_UNIT_EXISTENCE_OFF_FIELD)
        return -1;
    stats->entd_slot = battle_id;
    stats->existence = BATTLE_UNIT_EXISTENCE_ACTIVE;
    if (stats->team_flags & BATTLE_TEAM_MASK) {
        battle_unit_set_enemy_level_data(stats);
    }
    return 0;
}

/* Empties a unit slot and marks it off the field (defined but not placed). Always
 * returns 0. */
s32 battle_unit_set_cannot_exist(s32 battle_id) {
    unit_stats_t* unit;
    unit = &g_battle_unit_stats[battle_id];
    unit->entd_slot = BATTLE_ENTD_SLOT_NONE;
    unit->existence = BATTLE_UNIT_EXISTENCE_OFF_FIELD;
    return 0;
}

/* Empties a unit slot and sets its existence to pending removal, without testing
 * whether the unit is present. No retail code calls it. */
s32 battle_unit_erase_pending_party_removal(s32 unit_id) {
    unit_stats_t* unit = &g_battle_unit_stats[unit_id];
    unit->entd_slot = BATTLE_ENTD_SLOT_NONE;
    unit->existence = BATTLE_UNIT_EXISTENCE_PENDING_REMOVAL;
    return 0;
}

/* Move a unit to (x, y, level) with a new facing, carrying its mounted partner.
 *
 * A unit already standing on the destination becomes this unit's rider; any
 * stale mount link is dropped first.  Returns -1 without moving anything when
 * the destination tile is blocked or untargetable, 0 otherwise. */
s32 battle_unit_set_tile_position(s32 battle_id, u8 x, u8 y, u8 level, u8 facing) {
    unit_stats_t* unit;
    unit_stats_t* partner;
    s32 occupant;
    u8 rider;
    s32 has_rider;
    u8 mount_info;
    map_tile_flags_t tile_flags;

    has_rider = 0;
    unit = &g_battle_unit_stats[battle_id];
    occupant = battle_unit_find_at_tile(x, y, level, BATTLE_UNIT_TILE_FILTER_MOUNTABLE_CHOCOBO);
    if (occupant >= 0 && occupant != battle_id) {
        rider = occupant;
        has_rider = 1;
    }

    tile_flags = g_map_tile_data[(level << 8) + y * g_map_max_x + x].flags;
    if (tile_flags.blocked) {
        return -1;
    }
    if (tile_flags.untargetable) {
        return -1;
    }

    mount_info = unit->mount_info;
    if (!(mount_info & BATTLE_MOUNT_INFO_FLAG_MOUNT)) {
        unit->mount_info = 0;
        if (mount_info != 0) {
            partner = &g_battle_unit_stats[mount_info & BATTLE_MOUNT_INFO_PARTNER_ID_MASK];
            partner->mount_info = 0;
        }
    }
    if (has_rider != 0) {
        unit->mount_info = rider + BATTLE_MOUNT_INFO_FLAG_RIDER;
        g_battle_unit_stats[rider].mount_info = battle_id + BATTLE_MOUNT_INFO_FLAG_MOUNT;
    }

    unit->position.y = y;
    unit->x = x;
    unit->position.higher_elevation = level;
    unit->position.facing = facing;

    if (mount_info & BATTLE_MOUNT_INFO_FLAG_MOUNT) {
        partner = &g_battle_unit_stats[mount_info & BATTLE_MOUNT_INFO_PARTNER_ID_MASK];
        partner->position.y = y;
        partner->x = x;
        partner->position.higher_elevation = level;
        partner->position.facing = facing;
    }
    return 0;
}
