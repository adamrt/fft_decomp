/* Populate battle slots from party and ENTD records and validate unit placement. */

#include "fft/action.h"
#include "fft/battle_map.h"
#include "fft/deploy.h"
#include "fft/encounter.h"
#include "fft/move.h"
#include "fft/party.h"
#include "fft/reward.h"
#include "fft/script_variable.h"
#include "fft/sprite_sheet.h"
#include "fft/status.h"
#include "fft/system.h"
#include "fft/unit.h"
#include "fft/unit_entd.h"
#include "fft/unit_identity.h"
#include "fft/unit_party.h"
#include "fft/unit_runtime.h"
#include "psx/libc.h"
#include "psx/types.h"

/* Battle slots populated from party and ENTD records by the deployment routines below. */
unit_stats_t g_unit_stats[UNIT_SLOT_COUNT] = { 0 };

/* Deployment placement, the acting tile and strike relocation reservations. */
extern unit_deployment_coordinates_t* g_unit_deployed_coords;
extern s16 g_unit_entd_spritesheet_ids[];
extern s32 g_unit_entd_spritesheet_count;

static s32 entd_unique_spritesheet_add(s32 mode, s16 value, s32 limit);

static s32 placement_set_and_validate(s32 unit_idx, unit_deployment_coordinates_t* src);

static s32 placement_validate(s32 unit_idx);

/* Build battle slot unit_index from the party member named by g_unit_deployed_coords: rebuild it
 * from the party record, then clear its war trophy and bonus money and initialize its statuses. -1
 * when that party slot is unavailable. The target reads the slot from incoming a2; no caller contract
 * is known, so preserve the two preceding unused argument slots. */
s32 deploy_init_party_member(s32 unused0, s32 unused1, s32 unit_index) {
    unit_stats_t* unit = &g_unit_stats[unit_index];
    s32 formation_id = g_unit_deployed_coords->party_id;

    unit->battle_id = unit_index;
    if (unit_party_slot_init(unit, formation_id, 0) != 0) {
        return -1;
    }
    unit_status_rewards_init(unit, 1);
    return 0;
}

/* Initialize deployed units from map coordinates and return a failure bitmask.
 *
 * Normal deployment fills slots 0x10..0x14; red team debug deployment fills
 * slots 0..4 and clears the remaining ENTD slots. Bit i marks a load or
 * placement failure for row i; an empty row leaves its bit clear. A unit whose
 * identity is not a Ramza (1..3) gets a generated unit id, from 0x78 upward
 * (from 0xfe downward in debug mode). The two failure paths report
 * PARTY_UNIT_LOAD or PARTY_PLACEMENT plus the slot. The target reloads each
 * row from g_unit_deployed_coords at its distinct uses.
 */
static s32 units_init(unit_deployment_coordinates_t* formation, s32 is_red_team_debug) {
    s32 failures = 0;
    s32 weight = 1;
    s32 i;
    s32 slot_step;
    s32 slot_start;
    s32 next_generated_unit_id;
    u8 palette;

    g_unit_deployed_coords = formation;
    if (is_red_team_debug) {
        next_generated_unit_id = 0xfe;
        slot_step = -1;
        slot_start = 0;
    } else {
        next_generated_unit_id = 0x78;
        slot_step = 1;
        slot_start = DEPLOY_UNIT_SLOT_FIRST;
    }
    for (i = 0; i < DEPLOY_UNIT_SLOT_COUNT; i++, weight *= 2) {
        s32 battle_id = i + slot_start;
        unit_stats_t* unit = &g_unit_stats[battle_id];
        u8 sprite;
        u32 identity;

        unit->battle_id = battle_id;
        if (g_unit_deployed_coords[i].party_id != PARTY_ID_NONE) {
            unit->active_battle_id = battle_id;
            unit->existence = UNIT_EXISTENCE_ACTIVE;
            if (unit_party_slot_init(unit, g_unit_deployed_coords[i].party_id, 0) != 0) {
                system_battle_load_exception_handle(battle_id + SYSTEM_BATTLE_LOAD_EXCEPTION_PARTY_UNIT_LOAD);
            } else if (placement_set_and_validate(battle_id, &g_unit_deployed_coords[i]) != 0) {
                system_battle_load_exception_handle(battle_id + SYSTEM_BATTLE_LOAD_EXCEPTION_PARTY_PLACEMENT);
            } else {
                sprite = unit_sprite_identity_set(unit, &palette);
                identity = unit->character_identity;
                if ((identity & 0xff) == 0 || (identity & 0xff) >= 4) {
                    identity = next_generated_unit_id;
                    next_generated_unit_id += slot_step;
                }
                unit->unit_id = identity;
                if (is_red_team_debug) {
                    unit->team_flags = UNIT_TEAM_RED | UNIT_TEAM_FLAG_PLAYER_CONTROLLED;
                    unit->initial_team_flags = UNIT_TEAM_RED | UNIT_TEAM_FLAG_PLAYER_CONTROLLED;
                    unit->sprite_palette = 1;
                }
                if (g_unit_deployed_coords[i].flags.off_field) {
                    unit->existence = UNIT_EXISTENCE_OFF_FIELD;
                    unit->active_battle_id = UNIT_ACTIVE_BATTLE_ID_NONE;
                } else {
                    sprite_unit_graphics_load_queue(unit->x, unit->position.y, unit->position.higher_elevation,
                        unit->position.facing, sprite, palette, 0xff, unit, 0);
                    unit_status_rewards_init(unit, 1);
                    unit->existence = UNIT_EXISTENCE_ACTIVE;
                }
                continue;
            }
            failures += weight;
        }
        unit->active_battle_id = UNIT_ACTIVE_BATTLE_ID_NONE;
        unit->existence = UNIT_EXISTENCE_NONE;
    }
    if (is_red_team_debug) {
        for (i = DEPLOY_UNIT_SLOT_COUNT; i < ENTD_UNITS_PER_ENCOUNTER; i++) {
            unit_stats_t* unit = &g_unit_stats[i];
            unit->battle_id = i;
            unit->active_battle_id = UNIT_ACTIVE_BATTLE_ID_NONE;
            unit->existence = UNIT_EXISTENCE_NONE;
        }
    }
    return failures;
}

/* Clears g_reward_enemy_level_sum, then runs units_init
 * as the red team's debug deployment. The second argument is ignored. */
void deploy_init_units_for_debug_red_team(unit_deployment_coordinates_t* formation, s32 unused_1) {
    g_reward_enemy_level_sum = 0;
    units_init(formation, 1);
}

/* units_init for a normal deployment. The second
 * argument is ignored. */
s32 deploy_build_units(unit_deployment_coordinates_t* formation, s32 unused_1) {
    return units_init(formation, 0);
}

/* Store the deployment position and return its placement validation result. */
static s32 placement_set_and_validate(s32 unit_idx, unit_deployment_coordinates_t* src) {
    unit_stats_t* unit = &g_unit_stats[unit_idx];

    unit->position.higher_elevation = src->flags.upper_level;
    unit->position.y = src->y;
    unit->x = src->x;
    unit->position.facing = src->flags.facing;
    return placement_validate(unit_idx);
}

/* Validate a unit's placement tile.
 *
 * Returns -1 when the position is off the map, the tile is unwalkable, too
 * steep or deep for the unit's movement abilities, lava or an obstacle, or
 * already occupied by an earlier unit slot at the same elevation; 0 otherwise. */
static s32 placement_validate(s32 unit_idx) {
    unit_stats_t* unit = &g_unit_stats[unit_idx];
    unit_stats_t* other;
    battle_map_tile_t* tile;
    u8 x;
    u8 y;
    u8 higher_elevation;
    s32 i;

    x = unit->x;
    y = unit->position.y;
    higher_elevation = unit->position.higher_elevation;
    if (x >= g_battle_map_terrain_width) {
        return -1;
    }
    if (y >= g_battle_map_terrain_depth) {
        return -1;
    }
    tile = &g_battle_map_tile_data[battle_map_tile_calculate_unit_index(unit)];
    /* A byte collision test retains the original single-mask branch. */
    if (((*(u8*)&tile->flags) & BATTLE_MAP_TILE_COLLISION_MASK)) {
        return -1;
    }
    if (tile->depth_half_height.half_height >= 3) {
        return -1;
    }
    if ((unit->ability_flags.movement[1] & UNIT_MOVEMENT_SET_2_CANNOT_ENTER_WATER) && tile->depth_half_height.depth != 0
        && !(unit->ability_flags.movement[2] & UNIT_MOVEMENT_SET_3_FLOAT)) {
        return -1;
    }
    if (tile->depth_half_height.depth >= 3
        && !(unit->ability_flags.movement[2]
            & (UNIT_MOVEMENT_SET_3_MOVE_IN_WATER | UNIT_MOVEMENT_SET_3_WALK_ON_WATER
                | UNIT_MOVEMENT_SET_3_MOVE_UNDERWATER | UNIT_MOVEMENT_SET_3_FLOAT))) {
        return -1;
    }
    if (tile->surface.type == BATTLE_MAP_SURFACE_LAVA || tile->surface.type == BATTLE_MAP_SURFACE_OBSTACLE) {
        return -1;
    }
    for (i = 0; i < unit_idx; i++) {
        other = &g_unit_stats[i];
        if (other->active_battle_id != UNIT_ACTIVE_BATTLE_ID_NONE && other->x == x && other->position.y == y
            && other->position.higher_elevation == higher_elevation) {
            return -1;
        }
    }
    return 0;
}

/* Load the 16 ENTD units of the encounter into the battle unit slots.
 *
 * Each present unit is initialized; ENTD unit id 0xff takes a generated id
 * counting down from 0xfe. A unit whose presence bits are clear, or randomly
 * present on a failed coin flip, stays off the field. A unit that would need a
 * new spritesheet beyond the sheets left for the ENTD (nine less the
 * deployment unit limit) is taken off the field, and the units placed on the
 * field are capped at that number + 7; a unit with both presence bits set
 * loads its graphics but waits off the field and does not count. The
 * exception reports use the error bases 600, 300, 100 and 200 plus the slot.
 * `status` holds both the initialization result and the graphics descriptor's
 * not-deployed flag: sharing the one variable is what gives the flag enough
 * uses to win s1 ahead of battle_id. */
void unit_entd_load(const entd_encounter_t* entd_data, s32 mode) {
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
    const entd_unit_t* entd;
    const entd_unit_t* entd_list = entd_data->units;

    g_reward_player_unit_fallen = 0;
    g_reward_enemy_level_sum = 0;
    g_reward_highest_enemy_level = 0;
    party_highest_level_calculate();
    free_spritesheets = script_variable_get(EVENT_SCRIPT_VAR_DEPLOYMENT_UNIT_LIMIT);
    free_spritesheets = (free_spritesheets < 10) ? 9 - free_spritesheets : 0;
    enemy_limit = free_spritesheets + 7;
    enemy_count = 0;
    entd_unique_spritesheet_add(0xff, 0, 0);
    for (battle_id = 0, entd = entd_list; battle_id < ENTD_UNITS_PER_ENCOUNTER; entd++, battle_id++) {
        unit = &g_unit_stats[battle_id];
        unit->battle_id = battle_id;
        g_encounter_current_unit = entd;
        if (entd->sprite_set == CHARACTER_IDENTITY_ENTD_NONE) {
            unit->active_battle_id = UNIT_ACTIVE_BATTLE_ID_NONE;
            unit->existence = UNIT_EXISTENCE_NONE;
            continue;
        }

        unit->active_battle_id = battle_id;
        unit->existence = UNIT_EXISTENCE_ACTIVE;
        status = unit_encounter_init(unit, g_encounter_current_unit, 0, UNIT_ENTD_INIT_NEW);
        if (status != 0) {
            system_battle_load_exception_handle(battle_id + SYSTEM_BATTLE_LOAD_EXCEPTION_ENTD_UNIT_INIT);
        }
        spritesheet_id = unit_sprite_identity_set(unit, &palette_id);
        unit_status_rewards_init(unit, 0);
        /* s32 so the fallback id is zero-extended once here and passed on
         * unmasked, like the target. */
        unit_id = g_encounter_current_unit->unit_id;
        if (unit_id == 0xff) {
            unit_id = next_unused_unit_id--;
        }
        unit->unit_id = unit_id;

        presence = unit->team_flags & UNIT_TEAM_PRESENCE_MASK;
        if (presence == 0 || (presence == UNIT_TEAM_FLAG_RANDOMLY_PRESENT && (rand() & 1))) {
            unit->active_battle_id = UNIT_ACTIVE_BATTLE_ID_NONE;
            unit->existence = UNIT_EXISTENCE_OFF_FIELD;
            unit_position_rewards_init(unit, g_encounter_current_unit);
            continue;
        }

        unit_position_rewards_init(unit, g_encounter_current_unit);
        if (placement_validate(battle_id) != 0) {
            system_battle_load_exception_handle(battle_id + SYSTEM_BATTLE_LOAD_EXCEPTION_ENTD_PLACEMENT);
            unit->active_battle_id = UNIT_ACTIVE_BATTLE_ID_NONE;
            unit->existence = UNIT_EXISTENCE_NONE;
            continue;
        }
        if (unit->active_battle_id == UNIT_ACTIVE_BATTLE_ID_NONE) {
            continue;
        }
        if (entd_unique_spritesheet_add(battle_id, spritesheet_id, free_spritesheets) != 0) {
            unit->active_battle_id = UNIT_ACTIVE_BATTLE_ID_NONE;
            unit->existence = UNIT_EXISTENCE_OFF_FIELD;
            if (duplicate_message_shown != 0) {
                continue;
            }
            system_battle_load_exception_handle(battle_id + SYSTEM_BATTLE_LOAD_EXCEPTION_NO_SPRITESHEET_SLOT);
            duplicate_message_shown = 1;
            continue;
        }

        if (presence != UNIT_TEAM_PRESENCE_MASK && enemy_count >= enemy_limit) {
            unit->active_battle_id = UNIT_ACTIVE_BATTLE_ID_NONE;
            unit->existence = UNIT_EXISTENCE_NONE;
            if (limit_message_shown != 0) {
                continue;
            }
            system_battle_load_exception_handle(battle_id + SYSTEM_BATTLE_LOAD_EXCEPTION_ENEMY_LIMIT);
            limit_message_shown = 1;
            enemy_count++;
            continue;
        }
        if (presence == UNIT_TEAM_PRESENCE_MASK) {
            unit->active_battle_id = UNIT_ACTIVE_BATTLE_ID_NONE;
            unit->existence = UNIT_EXISTENCE_OFF_FIELD;
            status = 1;
        } else {
            status = 0;
            reward_count_enemy_level(unit);
        }
        sprite_unit_graphics_load_queue(unit->x, unit->position.y, unit->position.higher_elevation,
            unit->position.facing, spritesheet_id, palette_id, unit_id, unit, status);
        if (presence != UNIT_TEAM_PRESENCE_MASK) {
            enemy_count++;
        }
    }
}

/* Keep the list of distinct spritesheets the ENTD units need.
 *
 * Mode 0xff empties the list and mode 0 restarts it with value; any other mode
 * (the unit's slot) adds value when it is not listed yet. Returns 1 when value's
 * position in the list is limit or more (no spritesheet is left for it), else 0.
 * As slot 0 restarts the list, the first ENTD unit's sheet is always listed. */
static s32 entd_unique_spritesheet_add(s32 mode, s16 value, s32 limit) {
    s32 index;
    s32 i;

    if (mode == 0xFF) {
        g_unit_entd_spritesheet_count = 0;
        return 0;
    }
    if (mode == 0) {
        g_unit_entd_spritesheet_count = 1;
        g_unit_entd_spritesheet_ids[0] = value;
        return 0;
    }
    index = 0xFF;
    for (i = 0; i < g_unit_entd_spritesheet_count; i++) {
        if (g_unit_entd_spritesheet_ids[i] == value) {
            index = i;
            break;
        }
    }
    if (index == 0xFF) {
        index = g_unit_entd_spritesheet_count;
        g_unit_entd_spritesheet_count = index + 1;
        g_unit_entd_spritesheet_ids[index] = value;
    }
    return index >= limit;
}

/* Take a present unit out of the battle: the slot is emptied and its existence
 * set to UNIT_EXISTENCE_DISABLED. Always returns 0. */
s32 unit_disable(s32 battle_id) {
    unit_stats_t* unit = &g_unit_stats[battle_id];

    if (unit->active_battle_id != UNIT_ACTIVE_BATTLE_ID_NONE) {
        unit->active_battle_id = UNIT_ACTIVE_BATTLE_ID_NONE;
        unit->existence = UNIT_EXISTENCE_DISABLED;
    }
    return 0;
}

/* Put an off-field unit on the field: the slot becomes occupied, its existence
 * active, and a unit of any team but blue is counted into the enemy level
 * statistics.
 *
 * Returns 0, also for a unit already present, or -1 when the slot is empty in any
 * state other than off-field. */
s32 unit_put_on_field(u32 battle_id) {
    unit_stats_t* stats = &g_unit_stats[battle_id];

    if (stats->active_battle_id != UNIT_ACTIVE_BATTLE_ID_NONE)
        return 0;
    if (stats->existence != UNIT_EXISTENCE_OFF_FIELD)
        return -1;
    stats->active_battle_id = battle_id;
    stats->existence = UNIT_EXISTENCE_ACTIVE;
    if (stats->team_flags & UNIT_TEAM_MASK) {
        reward_count_enemy_level(stats);
    }
    return 0;
}

/* Empty a unit slot and mark it off the field (defined but not placed). Always
 * returns 0. */
s32 unit_set_off_field(s32 battle_id) {
    unit_stats_t* unit = &g_unit_stats[battle_id];

    unit->active_battle_id = UNIT_ACTIVE_BATTLE_ID_NONE;
    unit->existence = UNIT_EXISTENCE_OFF_FIELD;
    return 0;
}

/* Empty a unit slot and set its existence to pending removal, without testing
 * whether the unit is present. No retail code calls it. */
s32 unit_set_pending_removal(s32 unit_id) {
    unit_stats_t* unit = &g_unit_stats[unit_id];

    unit->active_battle_id = UNIT_ACTIVE_BATTLE_ID_NONE;
    unit->existence = UNIT_EXISTENCE_PENDING_REMOVAL;
    return 0;
}

/* Move a unit to (x, y, level) with a new facing, carrying its mounted partner.
 *
 * A mountable Chocobo already on the destination becomes this unit's mount: this
 * unit takes UNIT_MOUNT_INFO_FLAG_RIDER and the Chocobo UNIT_MOUNT_INFO_FLAG_MOUNT.
 * A unit that is not a mount drops its old link first; a mount carries its rider
 * along. Returns -1 without moving anything when the destination tile is blocked
 * or untargetable, 0 otherwise. */
s32 unit_set_tile_position(s32 battle_id, u8 x, u8 y, u8 layer, u8 facing) {
    unit_stats_t* unit;
    unit_stats_t* partner;
    s32 occupant;
    u8 mount_id;
    s32 has_mount = 0;
    u8 mount_info;
    battle_map_tile_flags_t tile_flags;

    unit = &g_unit_stats[battle_id];
    occupant = unit_find_at_tile(x, y, layer, UNIT_TILE_FILTER_MOUNTABLE_CHOCOBO);
    if (occupant >= 0 && occupant != battle_id) {
        mount_id = occupant;
        has_mount = 1;
    }

    tile_flags = g_battle_map_tile_data[(layer << 8) + y * g_battle_map_terrain_width + x].flags;
    if (tile_flags.blocked) {
        return -1;
    }
    if (tile_flags.untargetable) {
        return -1;
    }

    mount_info = unit->mount_info;
    if (!(mount_info & UNIT_MOUNT_INFO_FLAG_MOUNT)) {
        unit->mount_info = 0;
        if (mount_info != 0) {
            partner = &g_unit_stats[mount_info & UNIT_MOUNT_INFO_PARTNER_ID_MASK];
            partner->mount_info = 0;
        }
    }
    if (has_mount != 0) {
        unit->mount_info = mount_id + UNIT_MOUNT_INFO_FLAG_RIDER;
        g_unit_stats[mount_id].mount_info = battle_id + UNIT_MOUNT_INFO_FLAG_MOUNT;
    }

    unit->position.y = y;
    unit->x = x;
    unit->position.higher_elevation = layer;
    unit->position.facing = facing;

    if (mount_info & UNIT_MOUNT_INFO_FLAG_MOUNT) {
        partner = &g_unit_stats[mount_info & UNIT_MOUNT_INFO_PARTNER_ID_MASK];
        partner->position.y = y;
        partner->x = x;
        partner->position.higher_elevation = layer;
        partner->position.facing = facing;
    }
    return 0;
}

/* Collect the post-movement event flags (move_post_event_flags_e) for a unit that just moved: a
 * crystal or treasure on its tile; for a rider, the mount-removal statuses applied to its mount; a
 * Move-Find Item find (the mount's ability when riding) or else a trap; a canceled Charge; and the
 * Move-HP/MP Up and Move-Get EXP/JP flags. */
u32 move_get_post_events(unit_stats_t* unit) {
    unit_stats_t* target;
    u32 flags;
    s32 i;
    s32 find_item;
    move_find_result_data_t* tile;

    /* Synthesize CRYSTAL_OR_TREASURE when the filtered tile search returns a
     * non-negative unit ID. The explicit sign test keeps the target nor/srl. */
    flags = (~unit_find_at_tile(unit->x, unit->position.y, unit->position.higher_elevation,
                 UNIT_TILE_FILTER_CRYSTAL | UNIT_TILE_FILTER_TREASURE)
                >> 31)
        & 1;
    if (!(unit->mount_info & UNIT_MOUNT_INFO_FLAG_RIDER)) {
        target = unit;
    } else {
        target = &g_unit_stats[unit->mount_info & UNIT_MOUNT_INFO_PARTNER_ID_MASK];
        g_action_target = target;
        g_action_target_data = &target->action;
        action_clear_current_data(&target->action);
        for (i = 0; i < STATUS_BYTE_COUNT; i++) {
            g_action_target_data->status_changes.removal[i] = g_status_check_sets[STATUS_CHECK_SET_MOUNT_REMOVAL][i];
        }
        if (status_modify_inflictions(0) != 0) {
            g_action_target_data->attack_type = ACTION_TYPE_STATUS_CHANGE;
            status_resolve_unit_changes(target->battle_id, 0);
            flags |= MOVE_POST_EVENT_MOUNT_STATUS_CHANGE;
        }
    }
    find_item = target->ability_flags.movement[2] & UNIT_MOVEMENT_SET_3_MOVE_FIND_ITEM;
    tile = move_find_result_get(unit->x, unit->position.y, unit->position.higher_elevation);
    if (find_item && (tile->flags & MOVE_FIND_RESULT_ITEM)) {
        flags |= MOVE_POST_EVENT_ITEM_FOUND;
    } else if (tile->flags & MOVE_FIND_RESULT_TRAP) {
        flags |= MOVE_POST_EVENT_TRAP_TRIGGERED;
    }
    if (action_charge_check_and_cancel(unit, 0) != 0) {
        flags |= MOVE_POST_EVENT_CHARGING_CANCEL;
    }
    flags |= move_get_ability_events(unit);
    return flags;
}

/* Post-move benefits of the unit's movement abilities: Move-HP Up, Move-MP Up,
 * Move-Get EXP and Move-Get JP, each with the common benefit bit. */
s32 move_get_ability_events(const unit_stats_t* unit) {
    u8 flags = unit->ability_flags.movement[1];
    s32 post_event_flags = (0 - (unit->ability_flags.movement[0] & UNIT_MOVEMENT_SET_1_MOVE_HP_UP))
        & (MOVE_POST_EVENT_MOVEMENT_BENEFIT | MOVE_POST_EVENT_MOVE_HP_UP);
    if (flags & UNIT_MOVEMENT_SET_2_MOVE_MP_UP)
        post_event_flags |= MOVE_POST_EVENT_MOVEMENT_BENEFIT | MOVE_POST_EVENT_MOVE_MP_UP;
    if (flags & UNIT_MOVEMENT_SET_2_MOVE_GET_EXP)
        post_event_flags |= MOVE_POST_EVENT_MOVEMENT_BENEFIT | MOVE_POST_EVENT_MOVE_GET_EXP;
    if (flags & UNIT_MOVEMENT_SET_2_MOVE_GET_JP)
        post_event_flags |= MOVE_POST_EVENT_MOVEMENT_BENEFIT | MOVE_POST_EVENT_MOVE_GET_JP;
    return post_event_flags;
}
