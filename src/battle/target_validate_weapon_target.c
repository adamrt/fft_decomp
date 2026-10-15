#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_effect.h"
#include "fft/battle_map.h"
#include "fft/battle_status.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

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
s32 target_validate_weapon_target(const action_command_t* source) {
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
    target_id = target_find_unit_at_tile(x, y, elevation);
    if (target_id < 0) {
        target_id = -1;
    }
    if (flags & WEAPON_FLAG_ARC) {
        result = effect_set_and_validate_arc_trajectory(unit_id, &unit->action_command.target_position, target_id);
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
            result
                = effect_check_direct_trajectory_to_target(unit_id, &unit->action_command.target_position, target_id);
        }
    } else if (flags & WEAPON_FLAG_LUNGING) {
        result = target_validate_lunging_target(unit_id, x, y, elevation, target_id);
    } else {
        result = target_validate_height_overlap(unit_id, x, y, elevation, target_id);
    }
    target_disable_green_panel_flags();
    if (result >= 0 && result < BATTLE_UNIT_SLOT_COUNT) {
        target = &g_battle_unit_stats[result];
        tile = &g_map_tile_data[map_calculate_location(target)];
        tile->ceiling_depth_and_marks.targeted = 1;
    }
    return 0;
}
