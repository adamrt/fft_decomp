/* Random rolls, current-attacker record and Mime reactions. */

#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_formula.h"
#include "fft/battle_map.h"
#include "fft/battle_target.h"
#include "psx/types.h"

/* rand() when the action is executing; otherwise the midpoint 0x4000, so
 * previews and AI scoring see the average roll. */
s32 formula_get_random_0_7fff(void) {
    if (g_action_state != ACTION_STATE_EXECUTE) {
        return 0x4000;
    }
    return rand();
}

/* Return 1 when a random value in [0, limit) is at least threshold, otherwise 0. */
s32 formula_roll_random_at_least(s32 limit, s32 threshold) {
    return (formula_get_random_0_7fff() * limit) / 32768 >= threshold;
}

/* Empty. Called when the acting unit's own action has ended, with its stats and the quadrant
 * (0 to 3) its sprite faces. */
void action_post_action_facing_stub(unit_stats_t* unused_unit, s32 unused_facing) {
}

/* Record the acting unit's action for Mimes to copy: the action itself, the
 * target's offset from the unit, the unit's facing, team and weapons.
 *
 * A Mime's own action is not recorded. For a tile target only the low byte of
 * each coordinate is used. */
void action_set_current_attacker_data(unit_stats_t* unit) {
    unit_stats_t* target;
    u8 x;
    u8 y;

    if (unit->job_id == JOB_ID_MIME) {
        return;
    }
    main_util_copy_action_data(&unit->action_command, &g_action_current_attacker.action);
    if (unit->action_command.targeting_type == BATTLE_TARGETING_TYPE_UNIT) {
        target = &g_battle_unit_stats[unit->action_command.target_id];
        x = target->x;
        y = target->position.y;
    } else {
        /* Only the low byte of the s16 target coordinates is read. */
        x = unit->action_command.target_position.vx;
        y = unit->action_command.target_position.vz;
    }
    g_action_current_attacker.target_delta_x = x - unit->x;
    g_action_current_attacker.target_delta_y = y - unit->position.y;
    g_action_current_attacker.facing = unit->position.facing;
    g_action_current_attacker.team = unit->team_flags & BATTLE_TEAM_MASK;
    g_action_current_attacker.right_hand_weapon_id = unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON];
    g_action_current_attacker.left_hand_weapon_id = unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON];
    g_action_current_attacker.mimic_pending = 1;
}

/* Builds and previews a reaction using the current attacker's action and weapons.
 *
 * Returns 0 for an ineligible unit, -1 if targeting or preview fails, and 1
 * on success. Failed targeting or preview clears both borrowed weapon slots.
 * The menu type stays byte-sized, and the facing subtraction and action-field
 * assignments retain the target's evaluation order. The tile index and saved
 * action state share a temporary because their lifetimes do not overlap.
 */
s32 action_set_mimic_ability(unit_stats_t* unit) {
    /* A byte flag test retains the original compound-condition scheduling. */
    action_command_t action;
    action_command_t* current;
    s32 x;
    s32 y;
    s32 z;
    s32 direction;
    u8 menu_type;
    s32 result;
    s32 saved_state;
    s32 weapon_id;
    s32 unit_id;
    s32 attacker_facing;
    s32 ability_id;
    s32 facing;

    current = &g_action_current_attacker.action;
    if (action_is_reaction_prevented(unit) != 0) {
        return 0;
    }
    if ((unit->team_flags & BATTLE_TEAM_MASK) != g_action_current_attacker.team) {
        return 0;
    }
    if (unit->battle_id == current->actor_id) {
        return 0;
    }
    menu_type = g_main_action_menu_types_by_skillset[current->skillset_id];
    /* The target loads the ability id signed (lh). */
    ability_id = g_action_current_attacker.action.ability_id;
    if (menu_type == ACTION_MENU_TYPE_DEFAULT || menu_type == ACTION_MENU_TYPE_ARITHMETICKS
        || menu_type == ACTION_MENU_TYPE_MONSTER) {
        if (g_main_ability_range_data[ability_id].flags_3 & ABILITY_SECONDARY_FLAG_3_CANNOT_MIMIC) {
            return 0;
        }
    }
    facing = unit->position.facing;
    attacker_facing = g_action_current_attacker.facing - 4;
    direction = facing - attacker_facing;
    if (menu_type == ACTION_MENU_TYPE_JUMP) {
        direction |= 0x100;
    }
    if (map_resolve_rotated_offset_layer(unit, g_action_current_attacker.target_delta_x,
            g_action_current_attacker.target_delta_y, direction, &x, &y, &z)
        != 0) {
        return -1;
    }
    main_util_copy_action_data(current, &action);
    action.targeting_type = BATTLE_TARGETING_TYPE_TILE;
    action.actor_id = unit->battle_id;
    unit_id = unit->battle_id;
    action.target_position.vx = x;
    action.target_position.vy = z;
    weapon_id = g_action_current_attacker.right_hand_weapon_id;
    action.target_position.vz = y;
    action.target_id = unit_id;
    unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] = weapon_id;
    unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON] = g_action_current_attacker.left_hand_weapon_id;
    result = target_set_panels_for_action(&action);
    if (result == -1 || result == 3
        || (result < 2
            && !((*(u8*)&g_map_tile_data[saved_state = (z << 8) + (g_map_max_x * y) + x].ceiling_depth_and_marks)
                & MAP_TILE_FLAG_ABILITY_RANGE))) {
        unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] = ITEM_ID_NONE;
        unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON] = ITEM_ID_NONE;
        return -1;
    }
    saved_state = g_action_state;
    g_action_state = ACTION_STATE_PREVIEW;
    result = action_prepare_attack(&action, &unit->action_command, 1);
    unit->charged_ability_ct = 0xff;
    main_status_change_unit(unit, 0,
        STATUS_BYTE_MASK(STATUS_ID_CHARGING) | STATUS_BYTE_MASK(STATUS_ID_JUMP)
            | STATUS_BYTE_MASK(STATUS_ID_PERFORMING),
        MAIN_STATUS_REMOVE);
    g_action_state = saved_state;
    if (result < 0) {
        unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] = ITEM_ID_NONE;
        unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON] = ITEM_ID_NONE;
        return -1;
    }
    return 1;
}

/* Rotate a tile offset by the caller's facing and select its terrain layer.
 *
 * Returns -1 when the rotated offset leaves the map or lands on a blocked
 * lower tile whose upper tile is blocked too; otherwise 0 with `*out_flag`
 * selecting the upper (1) or lower (0) terrain layer.  Bit 8 of `flags` picks
 * the destination by absolute height instead of by proximity to the reference
 * tile's height.
 */
s32 map_resolve_rotated_offset_layer(
    unit_stats_t* unit, s32 dx, s32 dy, s32 flags, s32* out_x, s32* out_y, s32* out_flag) {
    map_tile_t* current;
    map_tile_t* lower;
    map_tile_t* upper;
    s32 index;
    s32 upper_index;
    s32 lower_distance;
    s32 upper_distance;
    s32 previous;
    u8 current_height;
    u8 lower_height;
    /* The upper tile's blocked bit, then its height: one byte variable, which is
     * why the first test of the bit keeps its andi 0xff. */
    u8 upper_value;

    switch (flags & 3) {
    case 1:
        previous = dx;
        dx = dy;
        dy = -previous;
        break;
    case 2:
        dx = -dx;
        dy = -dy;
        break;
    case 3:
        previous = dx;
        dx = -dy;
        dy = previous;
        break;
    }
    *out_x = unit->x + dx;
    *out_y = unit->position.y + dy;
    if (*out_x < 0 || *out_x >= g_map_max_x) {
        return -1;
    }
    if (*out_y < 0 || *out_y >= g_map_max_y) {
        return -1;
    }
    current = &g_map_tile_data[map_calculate_location(unit)];
    index = g_map_max_x * *out_y + *out_x;
    upper_index = index + 0x100;
    upper = &g_map_tile_data[upper_index];
    lower = &g_map_tile_data[index];
    upper_value = upper->flags.blocked;
    if ((lower->flags.blocked) != 0) {
        if (upper_value != 0) {
            return -1;
        }
        *out_flag = 1;
        return 0;
    }
    if (upper_value != 0) {
        *out_flag = 0;
        return 0;
    }
    upper_value = upper->height;
    current_height = current->height;
    lower_height = lower->height;
    if ((flags & 0x100) != 0) {
        if (upper_value < lower_height) {
            *out_flag = 0;
            return 0;
        }
        *out_flag = 1;
        return 0;
    }
    if (lower_height > current_height) {
        lower_distance = lower_height - current_height;
    } else {
        lower_distance = current_height - lower_height;
    }
    if (upper_value > current_height) {
        upper_distance = upper_value - current_height;
    } else {
        upper_distance = current_height - upper_value;
    }
    if (upper_distance < lower_distance) {
        *out_flag = 1;
        return 0;
    }
    if (lower_distance < upper_distance) {
        *out_flag = 0;
        return 0;
    }
    if (upper_value < lower_height) {
        *out_flag = 0;
    } else {
        *out_flag = 1;
    }
    return 0;
}

/* Nothing in the game calls it. */
s16 action_load_last_used_ability(void) {
    return g_battle_reaction_unit_last_ability_id;
}
