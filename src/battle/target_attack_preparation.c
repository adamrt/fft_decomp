/* Target panels, validation and attack preparation. */

#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_effect.h"
#include "fft/battle_map.h"
#include "fft/battle_status.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/* Validates a copied action and runs the targeting routine for its menu type.
 *
 * Returns -1 for an absent actor, an out-of-range unit target or an invalid
 * ability, 2 for menu types with no targeting, otherwise 1 when the routine
 * returns 0, -1 when it returns -1, and 0 for any other result. The target
 * reads the actor's stats pointer in $a0 as action_get_elemental_ability_id's
 * argument, so it is declared with one here. */
s32 target_calculate_for_menu_types(const action_command_t* source) {
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
        result = target_run_calculator(&action);
        mode = 0;
        break;
    case ACTION_MENU_TYPE_ELEMENTS:
        ability_id = action_get_elemental_ability_id(unit);
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
        result = target_set_green_panels_for_action(&action);
    }
    if (mode == 2) {
        result = target_disable_green_panel_on_all_but_target_tile(&action);
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
s32 target_validate_lunging_target(s32 unit_id, s32 x, s32 y, s32 elevation, s32 target_id) {
    battle_unit_height_data_t height;
    unit_stats_t* unit;
    map_tile_t* lower;
    map_tile_t* upper;
    s32 unit_x;
    s32 unit_y;
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

    unit = &g_battle_unit_stats[unit_id];
    unit_x = unit->x;
    unit_y = unit->position.y;
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
        index = mid_y * g_map_max_x + mid_x;
        lower = &g_map_tile_data[index];
        upper_index = index + 0x100;
        upper = &g_map_tile_data[upper_index];
        lower_height = lower->height * 2 + lower->depth_half_height.half_height;
        upper_height = upper->height * 2 + upper->depth_half_height.half_height;
        lower_id = target_find_unit_at_tile(mid_x, mid_y, 0);
        upper_id = target_find_unit_at_tile(mid_x, mid_y, 1);
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
        target_id = target_find_unit_at_tile(mid_x, mid_y, layer);
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
s32 target_validate_height_overlap(s32 attacker_id, s32 target_x, s32 target_y, s32 target_elevation, s32 target_id) {
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
void target_disable_green_panel_flags(void) {
    map_tile_t* tile;
    s32 i;

    for (i = 0; i < 0x200; i++) {
        tile = &g_map_tile_data[i];
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
s32 target_run_calculator(const action_command_t* source) {
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
    s32 count;
    s32 hit;
    s32 i;
    s32 j;

    count = 0;
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
    target_disable_green_panel_flags();
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
            tile = &g_map_tile_data[map_calculate_location(unit)];
            count++;
            tile->ceiling_depth_and_marks.targeted = 1;
        }
    }
    return count;
}

/* Leave only the action's target tile marked as targeted. Returns 1, or -1
 * when that tile is off the map or blocked. */
s32 target_disable_green_panel_on_all_but_target_tile(const action_command_t* action) {
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
    if (x < g_map_max_x) {
        if (y < g_map_max_y) {
            if (level < 2) {
                tile = &g_map_tile_data[(level << 8) + y * g_map_max_x + x];
                if ((tile->flags.blocked) == 0) {
                    target_disable_green_panel_flags();
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
s32 target_set_green_panels_for_action(const action_command_t* action) {
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
    if (x >= g_map_max_x) {
        return -1;
    }
    if (y >= g_map_max_y || elevation >= 2) {
        return -1;
    }
    location = x;
    index = location;
    index = elevation * 256 + y * g_map_max_x + location;
    tile = &g_map_tile_data[index];
    if (tile->flags.blocked) {
        return -1;
    }
    height = tile->height * 2 + ((*(u8*)&tile->depth_half_height) & MAP_TILE_HALF_HEIGHT_MASK)
        + ((*(u8*)&tile->depth_half_height) >> 5) * 2;
    if (aoe == 0xff) {
        cursor = g_target_panels;
        for (i = 0; i < 0x200; i++) {
            cursor[i].remaining_range = 1;
            cursor[i].mark = 0;
        }
    } else {
        cursor = g_target_panels;
        for (i = 0; i < 0x200; i++) {
            cursor[i].remaining_range = 0;
            cursor[i].mark = 0;
        }
        panel = &g_target_panels[index];
        panel->remaining_range = aoe + 1;
        if (index < 0x100) {
            other_panel = &g_target_panels[index + 0x100];
            panel->mark = 1;
        } else {
            other_panel = &g_target_panels[index - 0x100];
            other_panel->mark = 1;
        }
        g_target_panels[index & 0xff].mark = 1;
        if (!(flags_1 & ABILITY_SECONDARY_FLAG_1_WEAPON_RANGE)) {
            panel->remaining_range = aoe + 1;
            other_panel->remaining_range = aoe + 1;
            target_spread_panels(aoe, 0);
            target_apply_vertical_tolerance(height, vertical, (flags_2 & ABILITY_SECONDARY_FLAG_2_TOP_DOWN) != 0);
            target_check_moldball_virus_depth(action_copy.ability_id);
        }
    }
    cursor = &g_target_panels[map_calculate_location(unit)];
    if (flags_2 & ABILITY_SECONDARY_FLAG_2_CANNOT_HIT_CASTER) {
        cursor->remaining_range = 0;
    }
    if (flags_2 & ABILITY_SECONDARY_FLAG_2_LINEAR_ATTACK) {
        target_build_directional_attack_panels(&action_copy, 1);
    }
    if (flags_2 & ABILITY_SECONDARY_FLAG_2_THREE_DIRECTIONS) {
        target_build_directional_attack_panels(&action_copy, 3);
    }
    if (flags_2 & (ABILITY_SECONDARY_FLAG_2_CAN_TARGET_ENEMIES | ABILITY_SECONDARY_FLAG_2_CAN_TARGET_ALLIES)) {
        target_apply_unit_team_eligibility(unit_id, flags_2 & ABILITY_SECONDARY_FLAG_2_CAN_TARGET_ALLIES,
            flags_2 & ABILITY_SECONDARY_FLAG_2_CAN_TARGET_ENEMIES, aoe == 0xff);
    }
    count = 0;
    if (!(flags_2 & (ABILITY_SECONDARY_FLAG_2_CAN_TARGET_ENEMIES | ABILITY_SECONDARY_FLAG_2_CAN_TARGET_ALLIES))) {
        tile = g_map_tile_data;
        for (i = 0, marked = g_target_panels; i < 0x200; i++) {
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
    tile = g_map_tile_data;
    for (i = 0, marked = g_target_panels; i < 0x200; i++) {
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
void target_apply_unit_team_eligibility(s32 raw_unit_id, u8 allow_allies, u8 allow_enemies, u8 aoe_is_0xff) {
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
        if (target_is_unit_untargetable_and_store_tile(i, &panel_index) == 0) {
            relation = team_flags ^ g_battle_unit_stats[i].initial_team_flags;
            if (actor_id == i) {
                relation = 0;
            }
            panel = &g_target_panels[panel_index];
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
        for (i = 0; i < 0x200; i++) {
            panel = &g_target_panels[i];
            if (panel->remaining_range != 0) {
                panel->mark = 1;
            }
        }
    }
}

/* Store a unit's tile and report whether targeting must reject the unit. */
s32 target_is_unit_untargetable_and_store_tile(s32 unit_id, s32* tile_index) {
    unit_stats_t* unit = &g_battle_unit_stats[unit_id];

    *tile_index = map_calculate_location(unit);
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
void target_build_directional_attack_panels(const action_command_t* source, u8 mode) {
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
        target_calculate_linear_attack_tiles(TARGET_LINE_EAST, x, y);
        if (mode == 3) {
            target_calculate_linear_attack_tiles(TARGET_LINE_NORTH, x, y);
            target_calculate_linear_attack_tiles(TARGET_LINE_SOUTH, x, y);
        }
    }
    if (target_x < x) {
        target_calculate_linear_attack_tiles(TARGET_LINE_WEST, x, y);
        if (mode == 3) {
            target_calculate_linear_attack_tiles(TARGET_LINE_NORTH, x, y);
            target_calculate_linear_attack_tiles(TARGET_LINE_SOUTH, x, y);
        }
    }
    if (y < target_y) {
        target_calculate_linear_attack_tiles(TARGET_LINE_NORTH, x, y);
        if (mode == 3) {
            target_calculate_linear_attack_tiles(TARGET_LINE_EAST, x, y);
            target_calculate_linear_attack_tiles(TARGET_LINE_WEST, x, y);
        }
    }
    if (target_y < y) {
        target_calculate_linear_attack_tiles(TARGET_LINE_SOUTH, x, y);
        if (mode == 3) {
            target_calculate_linear_attack_tiles(TARGET_LINE_WEST, x, y);
            target_calculate_linear_attack_tiles(TARGET_LINE_EAST, x, y);
        }
    }
    for (i = 0; i < 0x200; i++) {
        panel = &g_target_panels[i];
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
 * the table is g_target_panels. The walk stops
 * at the map edge or after 32 tiles, and any other direction value marks nothing.
 * `dir` is reused as the panel index. */
void target_calculate_linear_attack_tiles(s32 dir, s32 x, s32 y) {
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
        map_width = g_map_max_x;
        if (x >= map_width) {
            return;
        }
        if (y < 0) {
            return;
        }
        if (y >= g_map_max_y) {
            return;
        }
        dir = y * map_width + x;
        panel = &g_target_panel_data[dir];
        if (panel->remaining_range != 0) {
            panel->mark = 1;
        }
        dir += 0x100;
        panel = &g_target_panel_data[dir];
        y += dy;
        if (panel->remaining_range != 0) {
            panel->mark = 1;
        }
        i += 1;
        x += dx;
    } while (i < 0x20);
}

/* Build a unit's targeting-height record.
 *
 * For a non-Chocobo, another unit at the same x, y, and elevation supplies
 * terrain, status, and movement data. Body, Float, and water movement then
 * determine the effective height.
 *
 * `id` holds the ENTD slot and then the job, and `level` the elevation and
 * then the walking height. Each shared variable is required: `id` conflicts
 * with the 0xff constant in $v0, which puts the job load in $v1, and `level`
 * outranks `depth` in allocation, which gives it $v1 and `depth` $a1.
 */
battle_unit_height_data_t* battle_unit_calculate_height_data(battle_unit_height_data_t* out, s32 unit_id) {
    battle_unit_height_data_t height;
    unit_stats_t* unit;
    unit_stats_t* candidate;
    map_tile_t* tile;
    /* Byte access retains the original packed load and field-extraction order. */
    s32 offset;
    s32 i;
    s32 id;
    u8 flags;
    u8 x;
    u8 y;
    u8 level;
    s32 tile_height;
    s32 depth;
    s32 water_offset;

    offset = 0;
    unit = &g_battle_unit_stats[unit_id];
    if (unit_id >= 0 && unit_id < BATTLE_UNIT_SLOT_COUNT && (id = unit->entd_slot) != 0xff
        && !(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)] & STATUS_BYTE_MASK(STATUS_ID_CRYSTAL))
        && !(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_TREASURE)] & STATUS_BYTE_MASK(STATUS_ID_TREASURE))) {
        flags = unit_id;
        id = unit->job_id;
        if (id < JOB_ID_CHOCOBO || id > JOB_ID_RED_CHOCOBO)
            flags = unit_id | BATTLE_UNIT_HEIGHT_NON_CHOCOBO;
        x = unit->x;
        height.x = x;
        y = unit->position.y;
        height.y = y;
        level = unit->position.higher_elevation;
        height.elevation = level;
        for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
            candidate = &g_battle_unit_stats[i];
            if (i != unit_id && candidate->entd_slot != BATTLE_ENTD_SLOT_NONE && candidate->x == x
                && candidate->position.y == y && candidate->position.higher_elevation == level) {
                if (flags & BATTLE_UNIT_HEIGHT_NON_CHOCOBO)
                    unit = candidate;
                offset = 2;
                flags |= BATTLE_UNIT_HEIGHT_SHARED_TILE;
                break;
            }
        }
        height.unit_flags = flags;
        flags = unit->status_sets.current[2];
        if (flags & (STATUS_BYTE_MASK(STATUS_ID_CHICKEN) | STATUS_BYTE_MASK(STATUS_ID_FROG)))
            height.unit_height = offset + 4;
        else
            height.unit_height = offset + 6;
        flags = unit->ability_flags.movement[2];
        tile = &g_map_tile_data[map_calculate_location(unit)];
        tile_height = tile->height * 2 + ((*(u8*)&tile->depth_half_height) & MAP_TILE_HALF_HEIGHT_MASK);
        depth = (*(u8*)&tile->depth_half_height) >> MAP_TILE_DEPTH_SHIFT;
        level = tile_height;
        if (depth != 0) {
            if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FLOAT)] & STATUS_BYTE_MASK(STATUS_ID_FLOAT)) {
                water_offset = depth * 2 + 2;
                level = tile_height + water_offset;
            } else if (flags & BATTLE_MOVEMENT_SET_3_MOVE_IN_WATER) {
                level = tile_height + depth * 2;
            } else if (flags & BATTLE_MOVEMENT_SET_3_WALK_ON_WATER) {
                water_offset = depth * 2 - 2;
                level = tile_height + water_offset;
            }
        } else if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FLOAT)] & STATUS_BYTE_MASK(STATUS_ID_FLOAT)) {
            level = tile_height + 2;
        }
        height.walking_height = level;
        height.total_height = level + height.unit_height;
        if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DEAD)] & STATUS_BYTE_MASK(STATUS_ID_DEAD))
            height.unit_flags = BATTLE_UNIT_HEIGHT_UNAVAILABLE;
    } else {
        height.unit_flags = BATTLE_UNIT_HEIGHT_UNAVAILABLE;
    }
    *out = height;
    return out;
}

/* Validate the action in place with action_prepare_attack phase 0, which
 * consumes nothing and leaves the unit's turn flags alone. */
void action_call_attack_preparation(action_command_t* action) {
    action_prepare_attack(action, action, 0);
}

/* Run action_prepare_attack phase 1 on a command, writing the prepared
 * action into the acting unit's own action fields.
 *
 * An action that resolves immediately also ends the unit's Charging, Jump,
 * Defending and Performing statuses, but only in the primary action context.
 * Returns the battle_attack_preparation_e result. */
s32 action_call_attack_preparation_at_preview(action_command_t* command) {
    unit_stats_t* unit;
    s32 result;

    unit = &g_battle_unit_stats[command->actor_id];
    result = action_prepare_attack(command, &unit->action_command, 1);
    if (result == BATTLE_ATTACK_PREPARATION_IMMEDIATE && g_action_context == ACTION_CONTEXT_PRIMARY) {
        status_enable_disable_acting(unit);
    }
    return result;
}

/*
 * The one-trip loop around `skip = 1` counts that set one loop level deeper,
 * so global-alloc ranks `skip` above `persevere` and gives them s6/s7 as in
 * the target.
 */
s32 action_prepare_attack(action_command_t* source, action_command_t* dest, s32 phase) {
    action_command_t action;
    unit_stats_t* unit;
    ability_secondary_data_t* entry;
    s16 ability;
    u8* table;
    u8 jump_ct;
    u8 item;
    u8 ct;
    u8 persevere;
    s32 skip;
    u16 type;
    u16 multiplier;
    u8 support;

    main_util_copy_action_data(source, &action);
    if (action.actor_id >= BATTLE_UNIT_SLOT_COUNT) {
        return BATTLE_ATTACK_PREPARATION_INVALID;
    }
    unit = &g_battle_unit_stats[action.actor_id];
    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
        return BATTLE_ATTACK_PREPARATION_INVALID;
    }
    skip = 0;
    if (action.targeting_type == BATTLE_TARGETING_TYPE_UNIT) {
        if (action.target_id >= BATTLE_UNIT_SLOT_COUNT) {
            return BATTLE_ATTACK_PREPARATION_INVALID;
        }
        if (g_battle_unit_stats[action.target_id].entd_slot == BATTLE_ENTD_SLOT_NONE) {
            return BATTLE_ATTACK_PREPARATION_INVALID;
        }
    }
    switch (g_main_action_menu_types_by_skillset[action.skillset_id]) {
    case ACTION_MENU_TYPE_ITEM_INVENTORY:
        item = action.item_or_equipment;
        if (g_main_item_primary_data[item].type != ITEM_TYPE_CONSUMABLE) {
            return BATTLE_ATTACK_PREPARATION_INVALID;
        }
        main_util_copy_action_data(&action, dest);
        ability = item + (ABILITY_ID_ITEM_FIRST - ITEM_ID_CONSUMABLE_FIRST);
        dest->ability_id = ability;
        if (phase != 0 && (phase & 1)) {
            action_decrement_player_item_quantity(unit, item, 1);
            unit->action_taken = 1;
        }
        return BATTLE_ATTACK_PREPARATION_IMMEDIATE;
    case ACTION_MENU_TYPE_WEAPON_INVENTORY:
        ability = ABILITY_ID_THROW_FIRST;
        item = action.item_or_equipment;
        table = g_main_throw_ability_item_types - ABILITY_ID_THROW_FIRST;
        for (; ability < ABILITY_ID_JUMP_FIRST; ability++) {
            /* The index+base integer spelling of the biased table is byte-required. */
            if (*(u8*)(ability + (s32)table) == g_main_item_primary_data[item].type) {
                break;
            }
        }
        if (ability == ABILITY_ID_JUMP_FIRST) {
            return BATTLE_ATTACK_PREPARATION_INVALID;
        }
        main_util_copy_action_data(&action, dest);
        dest->ability_id = ability;
        if (phase != 0 && (phase & 1)) {
            action_decrement_player_item_quantity(unit, item, 1);
            unit->action_taken = 1;
        }
        return BATTLE_ATTACK_PREPARATION_IMMEDIATE;
    case ACTION_MENU_TYPE_ARITHMETICKS:
        ability = action.ability_id;
        type = action.calculator_type;
        multiplier = action.calculator_multiplier;
        if (type < ABILITY_ID_MATH_FIRST || type >= ABILITY_ID_REACTION_FIRST) {
            return BATTLE_ATTACK_PREPARATION_INVALID;
        }
        if (multiplier < ABILITY_ID_MATH_FIRST || multiplier >= ABILITY_ID_REACTION_FIRST) {
            return BATTLE_ATTACK_PREPARATION_INVALID;
        }
        if (ability < 0 || ability >= ABILITY_ID_ITEM_FIRST) {
            return BATTLE_ATTACK_PREPARATION_INVALID;
        }
        main_util_copy_action_data(&action, dest);
        if (phase != 0 && (phase & 1)) {
            unit->action_taken = 1;
        }
        return BATTLE_ATTACK_PREPARATION_IMMEDIATE;
    case ACTION_MENU_TYPE_ELEMENTS:
        action.ability_id = action_get_elemental_ability_id(unit);
        break;
    case ACTION_MENU_TYPE_KATANA_INVENTORY:
        item = action.item_or_equipment;
        if (item < ITEM_ID_ASURA_KNIFE || item > ITEM_ID_CHIRIJIRADEN) {
            return BATTLE_ATTACK_PREPARATION_INVALID;
        }
        ability = item + (ABILITY_ID_DRAW_OUT_ASURA - ITEM_ID_ASURA_KNIFE);
        action.ability_id = ability;
        if (phase & 1) {
            action_decrement_player_item_quantity(unit, item, 1);
        }
        break;
    case ACTION_MENU_TYPE_ATTACK:
        main_util_copy_action_data(&action, dest);
        if (phase != 0 && (phase & 1)) {
            unit->action_taken = 1;
        }
        dest->ability_id = 0;
        return BATTLE_ATTACK_PREPARATION_IMMEDIATE;
    case ACTION_MENU_TYPE_JUMP:
        main_util_copy_action_data(&action, dest);
        if (phase != 0) {
            ct = 50 / unit->attributes[UNIT_ATTRIBUTE_SPEED];
            if (ct == 0) {
                jump_ct = 1;
            } else {
                jump_ct = ct;
            }
            unit->charged_ability_ct = jump_ct;
            if (phase & 1) {
                unit->action_taken = 1;
                unit->movement_taken = 1;
                main_status_set_action_state(unit, MAIN_UNIT_ACTION_STATE_JUMPING);
            }
        }
        dest->ability_id = ABILITY_ID_JUMP_FIRST;
        return BATTLE_ATTACK_PREPARATION_JUMP;
    case ACTION_MENU_TYPE_CHARGE:
        ability = action.ability_id;
        if (ability < ABILITY_ID_CHARGE_FIRST || ability >= ABILITY_ID_MATH_FIRST) {
            return BATTLE_ATTACK_PREPARATION_INVALID;
        }
        main_util_copy_action_data(&action, dest);
        ct = g_main_charge_ability_data[ability - ABILITY_ID_CHARGE_FIRST].ct;
        do {
            skip = 1;
        } while (0);
        if (phase & 1) {
            unit->movement_taken = 1;
        }
        persevere = 0;
        break;
    case ACTION_MENU_TYPE_DEFEND:
        return BATTLE_ATTACK_PREPARATION_CHARGE;
    case ACTION_MENU_TYPE_CHANGE_EQUIPMENT:
        return BATTLE_ATTACK_PREPARATION_CHANGE_EQUIPMENT;
    case ACTION_MENU_TYPE_DEFAULT:
    case ACTION_MENU_TYPE_MONSTER:
        break;
    default:
        return BATTLE_ATTACK_PREPARATION_INVALID;
    }
    if (!skip) {
        ability = action.ability_id;
        if (ability < 0 || ability >= ABILITY_ID_ITEM_FIRST) {
            return BATTLE_ATTACK_PREPARATION_INVALID;
        }
        main_util_copy_action_data(&action, dest);
        entry = &g_main_ability_range_data[ability];
        ct = entry->ct & 0x7f;
        persevere = entry->flags_3 & ABILITY_SECONDARY_FLAG_3_PERSEVERE;
        support = unit->ability_flags.support[3];
        if (!persevere) {
            if (support & BATTLE_SUPPORT_SET_4_SHORT_CHARGE) {
                ct = (ct + 1) >> 1;
            }
            if (support & BATTLE_SUPPORT_SET_4_NON_CHARGE) {
                ct = 0;
            }
        }
    }
    if (phase != 0) {
        unit->charged_ability_ct = ct;
        if (phase & 1) {
            if (g_action_context != ACTION_CONTEXT_PRIMARY) {
                return ct == 0;
            }
            unit->action_taken = 1;
            if (ct != 0) {
                if (persevere) {
                    main_status_set_action_state(unit, MAIN_UNIT_ACTION_STATE_PERFORMING);
                } else {
                    main_status_set_action_state(unit, MAIN_UNIT_ACTION_STATE_CHARGING);
                }
            }
        }
    }
    return ct == 0;
}

/* Returns an occupied unit slot's action command, or 0 for an
 * empty slot or an id past the last slot. No retail code calls it. */
action_command_t* battle_unit_get_action_command_by_battle_id(s32 battle_id) {
    if (battle_id < BATTLE_UNIT_SLOT_COUNT) {
        if (g_battle_unit_stats[battle_id].entd_slot != BATTLE_ENTD_SLOT_NONE) {
            return &g_battle_unit_stats[battle_id].action_command;
        }
    }
    return 0;
}

/* Check, and optionally spend, one of an inventory item for a unit that started
 * the battle on the blue team; other teams have unlimited stock.
 *
 * Returns -1 when the party owns none, otherwise 0. One item is removed only when
 * always_one is nonzero and the action is executing (not a preview or an AI
 * simulation); with always_one 0 the call only tests the stock. */
s32 action_decrement_player_item_quantity(unit_stats_t* unit, s32 item_id, s32 always_one) {
    u32 idx;
    s32 qty;

    if ((unit->initial_team_flags & BATTLE_TEAM_MASK) == BATTLE_TEAM_BLUE) {
        idx = item_id & 0xFF;
        qty = g_main_item_quantities[idx];
        if (qty == 0) {
            return -1;
        }
        if (always_one == 0) {
            return 0;
        }
        if (g_action_state != ACTION_STATE_EXECUTE) {
            return 0;
        }
        g_main_item_quantities[idx] = qty - 1;
    }
    return 0;
}

/* Start resolving a unit's queued action: check and spend its MP, record it as
 * the acting unit with its last ability, clear every unit's reaction and outcome
 * and set up the first strike.
 *
 * Returns 0 when the action can run, -1 for an empty unit slot, 6 when Hamedo
 * pre-empted the action (g_action_hamedo_flag set), or the nonzero
 * rejection code of action_check_and_consume_mp; on those failures only
 * the acting-unit data is stored. */
s32 action_init_current_data(s32 id) {
    unit_stats_t* unit;
    s32 i;
    unit_stats_t* base;
    s32 mp_result;
    u16 ability;

    g_battle_acting_unit_id = -1;
    g_action_context = ACTION_CONTEXT_PRIMARY;
    if (id >= BATTLE_UNIT_SLOT_COUNT
        || (unit = &g_battle_unit_stats[id], base = g_battle_unit_stats, unit->entd_slot == BATTLE_ENTD_SLOT_NONE)) {
        return -1;
    }
    if (g_action_hamedo_flag != 0) {
        action_store_acting_unit_data(unit);
        return 6;
    }
    mp_result = action_check_and_consume_mp(unit);
    if (mp_result != 0) {
        action_store_acting_unit_data(unit);
        return mp_result;
    }
    /* The target loads the s16 action_command.ability_id unsigned (lhu). */
    ability = unit->action_command.ability_id;
    g_battle_acting_unit_id = id;
    g_battle_acting_unit_used_ability_id = ability;
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        base[i].action.reaction_id = 0;
        base[i].ability_outcome = 0;
    }
    action_init_current_ability_strike_data(unit);
    action_set_current_attacker_data(unit);
    return 0;
}

/*
 * Build the target list for a reaction ability.
 *
 * Reflect redirects to the unit on the action tile, Distribute selects
 * injured allies, and Damage Split selects the acting unit. The remaining
 * reactions follow the target policy stored in the reaction flags table.
 * Returns 0 only for a reaction that targets the attacker.
 */
s32 action_build_reaction_targets(s32 actor_id, battle_strike_work_t* work, s32* target_count, u8* target_ids) {
    unit_stats_t* actor = &g_battle_unit_stats[actor_id];
    unit_stats_t* unit_base = g_battle_unit_stats;
    unit_stats_t* unit;
    s32 tile_index;
    /* One variable: the Reflect target id, then the Distribute target count. */
    s32 target_or_count;
    u8 flags;

    switch (g_battle_current_reaction_ability_id) {
    case ABILITY_ID_REACTION_REFLECT: {
        u8 target_x = actor->action_command.target_position.vx;
        u8 target_y;
        u8 target_elevation;
        u8 mount_info;

        work->target_new_x = target_x;
        target_y = actor->action_command.target_position.vz;
        work->target_new_y = target_y;
        target_elevation = actor->action_command.target_position.vy;
        work->target_new_map_level = target_elevation;
        target_or_count = target_find_unit_at_tile(target_x, target_y, target_elevation);
        if (target_or_count < 0) {
            *target_count = 0;
            break;
        }
        unit = &unit_base[target_or_count];
        mount_info = unit->mount_info;
        if (mount_info & BATTLE_MOUNT_INFO_FLAG_MOUNT) {
            target_or_count = mount_info & BATTLE_MOUNT_INFO_PARTNER_ID_MASK;
        }
        if (target_is_unit_untargetable_and_store_tile(target_or_count, &tile_index) != 0) {
            *target_count = 0;
            break;
        }
        *target_count = 1;
        *target_ids = target_or_count;
        break;
    }

    case ABILITY_ID_REACTION_DISTRIBUTE: {
        s32 unit_id;
        s32 actor_team;

        target_or_count = 0;
        actor_team = actor->team_flags & BATTLE_TEAM_MASK;
        for (unit_id = 0; unit_id < BATTLE_UNIT_SLOT_COUNT; unit_id++) {
            if (unit_id == actor_id) {
                continue;
            }
            unit = &unit_base[unit_id];
            if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
                continue;
            }
            if (unit->mount_info & BATTLE_MOUNT_INFO_FLAG_MOUNT) {
                continue;
            }
            if (unit->hp == unit->max_hp) {
                continue;
            }
            if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)]
                & (STATUS_BYTE_MASK(STATUS_ID_CRYSTAL) | STATUS_BYTE_MASK(STATUS_ID_DEAD)
                    | STATUS_BYTE_MASK(STATUS_ID_JUMP))) {
                continue;
            }
            if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_PETRIFY)]
                & (STATUS_BYTE_MASK(STATUS_ID_PETRIFY) | STATUS_BYTE_MASK(STATUS_ID_TREASURE))) {
                continue;
            }
            /* Retail keeps the candidate's team in the tile-index stack slot. */
            tile_index = unit->initial_team_flags & BATTLE_TEAM_MASK;
            if (tile_index == actor_team) {
                target_ids[target_or_count++] = unit_id;
            }
        }
        g_battle_distribute_target_count = target_or_count;
        *target_count = target_or_count;
        break;
    }

    case ABILITY_ID_REACTION_DAMAGE_SPLIT:
        unit = &unit_base[g_battle_acting_unit_id];
        if (unit->entd_slot != BATTLE_ENTD_SLOT_NONE) {
            *target_count = 1;
            *target_ids = g_battle_acting_unit_id;
        } else {
            *target_count = 0;
        }
        break;

    default:
        flags = g_main_reaction_behavior_flags[g_battle_current_reaction_ability_id - ABILITY_ID_REACTION_FIRST];
        if (flags & BATTLE_REACTION_BEHAVIOR_FLAG_TARGET_SELF) {
            *target_count = 1;
            *target_ids = actor_id;
            break;
        }
        if (flags & BATTLE_REACTION_BEHAVIOR_FLAG_TARGET_ATTACKER) {
            return 0;
        }
        *target_count = 0;
        break;
    }
    return 1;
}

/*
 * Compute target tiles for the action and report its ability flags_3 byte.
 *
 * Attack, Charge, item-inventory, weapon-inventory, direct-targeting, and
 * weapon-range actions use the weapon/direct resolver. Random-fire selection
 * runs only after successful ordinary targeting outside AI simulation.
 */
s32 target_calculate_map_for_action(const action_command_t* action, u8* flags_3) {
    ability_secondary_data_t* ability;
    u8 menu_type;
    u8 flags_4;
    u8 flags_1;
    u8 flags_2;
    s32 result;
    s16 ability_id;

    flags_4 = 0;
    flags_1 = 0;
    menu_type = g_main_action_menu_types_by_skillset[action->skillset_id];
    flags_2 = 0;
    *flags_3 = 0;
    if (menu_type == ACTION_MENU_TYPE_DEFAULT || menu_type == ACTION_MENU_TYPE_MONSTER) {
        ability_id = action->ability_id;
        if (ability_id < ABILITY_ID_ITEM_FIRST) {
            ability = &g_main_ability_range_data[ability_id];
            *flags_3 = ability->flags_3;
            flags_4 = ability->flags_4;
            flags_1 = ability->flags_1;
            flags_2 = ability->flags_2;
        }
    }
    if (menu_type == ACTION_MENU_TYPE_ATTACK || menu_type == ACTION_MENU_TYPE_CHARGE
        || menu_type == ACTION_MENU_TYPE_ITEM_INVENTORY || menu_type == ACTION_MENU_TYPE_WEAPON_INVENTORY
        || (flags_4 & ABILITY_SECONDARY_FLAG_4_DIRECT_TARGETING) || (flags_1 & ABILITY_SECONDARY_FLAG_1_WEAPON_RANGE)) {
        result = target_validate_weapon_target(action);
    } else {
        result = target_calculate_for_menu_types(action);
        if (result != -1 && (flags_2 & ABILITY_SECONDARY_FLAG_2_RANDOM_FIRE)
            && g_action_state == ACTION_STATE_EXECUTE) {
            target_select_random_tile_for_random_fire_abilities();
        }
    }
    return result;
}

/* Resolve one strike of the current ability: build the target list, run the
 * per-target formula setup and fill the strike work record.
 *
 * Returns -1 for an invalid actor or targeting, 1 when nothing was hit and 0
 * otherwise. After the clearing loop i holds 21, which the target reuses as
 * the Math skillset id in the control-value test. The (u16*)/(s16*) views
 * reproduce the target's load forms and memory-access ordering. */
s32 action_resolve_ability_strike(s32 battle_id, battle_strike_work_t* work) {
    u8 targets[BATTLE_UNIT_SLOT_COUNT];
    s32 count;
    u8 flags_3;
    unit_stats_t* attacker;
    s32 i;
    s32 reaction_targeted;
    u8 target_id;
    u16 reaction_id;
    map_tile_t* tile;

    g_battle_sort_targets_nearest_first = 0;
    g_battle_relocated_unit_count = 0;
    g_action_current_ability.random_fire_flag = 0;
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        targets[i] = 0xff;
    }
    if (battle_id >= BATTLE_UNIT_SLOT_COUNT) {
        return -1;
    }
    attacker = &g_battle_unit_stats[battle_id];
    action_clear_current_data(&attacker->action);
    attacker->action.hit = 0;
    attacker->action.miss_type = ACTION_MISS_TYPE_CANCELED;
    action_clear_current_data(&g_action_attacker_action_data);
    g_action_attacker_action_data.hit = 0;
    work->knockback_flags = 0;
    work->post_action_message = BATTLE_POST_ACTION_MESSAGE_NONE;
    work->math_skill_flag = 0;
    work->reaction_ability_id = 0;
    reaction_targeted = 0;
    if (g_action_context != ACTION_CONTEXT_PRIMARY) {
        work->reaction_ability_id = g_battle_current_reaction_ability_id;
        if (action_build_reaction_targets(battle_id, work, &count, targets)) {
            if (g_battle_current_reaction_ability_id == ABILITY_ID_REACTION_REFLECT) {
                flags_3 = ABILITY_SECONDARY_FLAG_3_ANIMATE_ON_MISS;
            } else {
                flags_3 = 0;
            }
            reaction_targeted = 1;
        }
    } else if (attacker->action_command.skillset_id == i) {
        work->math_skill_flag = 1;
    }
    if (!reaction_targeted) {
        if (target_calculate_map_for_action(&attacker->action_command, &flags_3) == -1) {
            work->target_count = 0;
            work->continue_attack = 0;
            return -1;
        }
        if (g_action_current_ability.post_action_target_id < BATTLE_UNIT_SLOT_COUNT
            && (g_action_current_ability.weapon_proc_pending != 0
                || (g_action_current_ability.knockback_flags & BATTLE_KNOCKBACK_PENDING))) {
            target_disable_green_panel_flags();
            count = 1;
            targets[0] = g_action_current_ability.post_action_target_id;
            for (i = 1; i < BATTLE_UNIT_RUNTIME_SLOT_COUNT; i++) {
                targets[i] = 0xff;
            }
            tile = &g_map_tile_data[map_calculate_location(
                &g_battle_unit_stats[g_action_current_ability.post_action_target_id])];
            tile->ceiling_depth_and_marks.targeted = 1;
        }
        count = target_list_units_on_panels(targets, attacker);
    }
    g_action_current_ability.target_count = count;
    action_store_target_ids(targets);
    action_store_used_weapon(&attacker->action_command);
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        target_id = targets[i];
        if (target_id != 0xff) {
            g_action_current_ability.defaulted_to_attack = 0;
            action_run_pre_formula_setup(&attacker->action_command, target_id);
        }
    }
    status_remove_transparent_if_jump_used(attacker);
    count = target_copy_ability_targets(work->target_list);
    work->target_count = count;
    g_action_current_ability.target_count = count;
    if (g_action_context == ACTION_CONTEXT_PRIMARY) {
        g_action_current_ability.can_earn_exp_jp = 1;
    }
    work->can_earn_experience = 0;
    action_finalize_draw_out_katana_result(attacker, work, count);
    work->actor_id = battle_id;
    work->used_weapon_id = g_action_current_ability.weapon_id;
    work->reaction_occurred = 0;
    work->current_hit_number = g_action_current_ability.strike_counter;
    target_set_coordinates_for_ability(attacker, work);
    if (count == 0) {
        g_action_current_ability.strike_counter++;
        work->last_attack_id = attacker->action_command.ability_id;
        work->reaction_id = 0;
        if (!(flags_3 & ABILITY_SECONDARY_FLAG_3_ANIMATE_ON_MISS)) {
            work->skip_empty_tile_effect = 1;
        }
        if (g_action_current_ability.strike_counter < g_action_current_ability.strike_count) {
            work->continue_attack = 1;
        } else {
            work->continue_attack = 0;
        }
        return 1;
    }
    work->post_action_message = g_action_current_ability.post_action_message;
    work->skip_empty_tile_effect = 0;
    reaction_id = g_action_current_ability.reaction_id;
    if (reaction_id != 0 && g_action_current_ability.weapon_proc_pending == 0) {
        work->last_attack_id = reaction_id;
        work->reaction_occurred = 1;
    } else if (g_action_current_ability.defaulted_to_attack != 0) {
        work->last_attack_id = 0;
    } else {
        work->last_attack_id = attacker->action_command.ability_id;
    }
    work->reaction_id = g_action_current_ability.reaction_id;
    if (g_action_current_ability.knockback_flags != 0) {
        work->knockback_flags = g_action_current_ability.knockback_flags & BATTLE_KNOCKBACK_KIND_MASK;
        work->target_new_x = g_action_current_ability.target_new_x;
        work->target_new_y = g_action_current_ability.target_new_y;
        work->target_new_map_level = g_action_current_ability.target_new_elevation;
    }
    if (g_action_current_ability.weapon_proc_pending != 0
        || (g_action_current_ability.knockback_flags & BATTLE_KNOCKBACK_PENDING)) {
        work->continue_attack = 1;
    } else {
        if (g_action_current_ability.knockback_flags != 0) {
            work->reaction_occurred = 1;
            work->skip_empty_tile_effect = 1;
            work->last_attack_id = ABILITY_ID_KNOCKBACK;
        }
        g_action_current_ability.strike_counter++;
        if (g_action_current_ability.strike_counter < g_action_current_ability.strike_count) {
            work->continue_attack = 1;
        } else {
            work->continue_attack = 0;
        }
    }
    return 0;
}

/* Store the tile a strike lands on in the strike work record: the random-fire
 * tile picked for this strike, else the target unit's tile and level, else the
 * action's target tile. skip_empty_tile_effect is cleared. */
void target_set_coordinates_for_ability(const unit_stats_t* acting, battle_strike_work_t* out) {
    unit_stats_t* target;

    out->skip_empty_tile_effect = 0;
    if (g_action_current_ability.random_fire_flag != 0) {
        out->target_new_x = g_action_current_ability.target_new_x;
        out->target_new_y = g_action_current_ability.target_new_y;
        out->target_new_map_level = g_action_current_ability.target_new_elevation;
    } else if (acting->action_command.targeting_type == BATTLE_TARGETING_TYPE_UNIT) {
        target = &g_battle_unit_stats[acting->action_command.target_id];
        out->target_new_x = target->x;
        out->target_new_y = target->position.y;
        out->target_new_map_level = target->position.higher_elevation;
    } else {
        out->target_new_x = acting->action_command.target_position.vx;
        out->target_new_y = acting->action_command.target_position.vz;
        out->target_new_map_level = acting->action_command.target_position.vy;
    }
}

/* Finalize Draw Out's katana-consumption result after target processing.
 *
 * hit_count is the number of units the strike hit; with none, or when the
 * katana did not break, the action becomes a pseudo-status hit. When the
 * katana broke, retail copies can_earn_experience onto itself (lbu/sb of the
 * same byte): a genuine no-op kept for the match. */
void action_finalize_draw_out_katana_result(unit_stats_t* attacker, battle_strike_work_t* work, u8 hit_count) {
    u16 flags;

    if (g_main_action_menu_types_by_skillset[attacker->action_command.skillset_id] != ACTION_MENU_TYPE_KATANA_INVENTORY)
        return;
    if (attacker->job_id == JOB_ID_MIME)
        return;
    if ((hit_count & 0xFF) == 0
        || ((flags = g_action_attacker_action_data.special_effect) & ACTION_SPECIAL_EFFECT_DRAW_OUT_KATANA_NOT_BROKEN)
            != 0) {
        g_action_attacker_action_data.special_effect = ACTION_SPECIAL_EFFECT_DRAW_OUT_KATANA_NOT_BROKEN;
        g_action_attacker_action_data.attack_type = ACTION_TYPE_PSEUDO_STATUS;
        g_action_attacker_action_data.hit = 1;
        return;
    }
    if (flags & ACTION_SPECIAL_EFFECT_DRAW_OUT_KATANA_BROKEN) {
        work->can_earn_experience = work->can_earn_experience;
    }
}

/*
 * Seed the per-action "strike" state from the acting unit before an attack
 * resolves: reset the strike counter, elemental and knockback scratch, clear
 * the experience/level-up flags when no action context is live, latch the
 * unit's two weapon slots, and decide how many strikes the action gets
 * (g_action_current_ability.strike_count). Two-swords and the 0x20 ability flag are what allow a second
 * strike; menu type 2 (item) only records the consumed item.
 *
 * The `ability_id >= 0 || ability_id < ABILITY_ID_ITEM_FIRST` guard is the target's own: the
 * second arm is unreachable, so the table read is never actually skipped.
 *
 * `left_hand` is load-bearing: the target issues the equipment[5] reload
 * BEFORE the 0xff store, which costs it the menu-type zero-extension in $v1
 * and makes it re-emit `andi v1,a2,0xff` after the block. Reading the slot
 * straight into the global instead sinks the load past the store and drops
 * that instruction.
 */
void action_init_current_ability_strike_data(unit_stats_t* unit) {
    ability_secondary_data_t* data;
    u8 menu_type;
    u8 weapon_kind;
    u8 chance;
    s16 ability_id;
    u8 support;
    u8 left_hand;

    g_action_current_ability.strike_count = 1;
    g_action_current_ability.strike_counter = 0;
    g_action_current_ability.weapon_proc_pending = 0;
    g_action_current_ability.knockback_flags = 0;
    weapon_kind = 0;
    if (g_action_context == ACTION_CONTEXT_PRIMARY) {
        g_action_current_ability.can_earn_exp_jp = 0;
        g_action_current_ability.earned_experience = 0;
        g_action_current_ability.level_gained_flag = 0;
        g_action_current_ability.job_level_gained_flag = 0;
    }

    menu_type = g_main_action_menu_types_by_skillset[unit->action_command.skillset_id];
    g_battle_loaded_ability_flags_1 = 0;
    g_action_current_ability.primary_weapon_id = unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON];
    g_action_current_ability.secondary_weapon_id = unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON];
    if (g_action_current_ability.primary_weapon_id == ITEM_ID_NONE) {
        left_hand = unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON];
        g_action_current_ability.secondary_weapon_id = ITEM_ID_NONE;
        g_action_current_ability.primary_weapon_id = left_hand;
    }

    if (menu_type == ACTION_MENU_TYPE_WEAPON_INVENTORY) {
        g_action_current_ability.primary_weapon_id = unit->action_command.item_or_equipment;
        return;
    }

    if (menu_type == ACTION_MENU_TYPE_DEFAULT || menu_type == ACTION_MENU_TYPE_MONSTER) {
        ability_id = unit->action_command.ability_id;
        if (ability_id >= 0 || ability_id < ABILITY_ID_ITEM_FIRST) {
            data = &g_main_ability_range_data[ability_id];
            g_battle_loaded_ability_flags_1 = data->flags_1;
            chance = data->x;
            weapon_kind = data->formula;
        }
        if (weapon_kind == ABILITY_FORMULA_MULTIHIT_TRUTH_MAGIC
            || weapon_kind == ABILITY_FORMULA_UNFAITH_MAGICAL_DAMAGE) {
            if (g_action_state == ACTION_STATE_EXECUTE) {
                g_action_current_ability.strike_count = ((chance * rand()) / 32768) + 1;
                return;
            }
        }
        if (weapon_kind == ABILITY_FORMULA_DAMAGE_MA_PLUS_Y_HALF_TIMES_MA) {
            g_action_current_ability.strike_count = chance + 1;
            return;
        }
    }

    if (menu_type == ACTION_MENU_TYPE_JUMP) {
        return;
    }
    if (menu_type != ACTION_MENU_TYPE_ATTACK && menu_type != ACTION_MENU_TYPE_CHARGE) {
        if ((g_battle_loaded_ability_flags_1 & ABILITY_SECONDARY_FLAG_1_WEAPON_RANGE) == 0) {
            return;
        }
    }

    support = unit->ability_flags.support[2];
    if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FROG)] & STATUS_BYTE_MASK(STATUS_ID_FROG))
        || (unit->unit_flags & UNIT_FLAG_MONSTER)) {
        g_action_current_ability.primary_weapon_id = ITEM_ID_NONE;
        g_action_current_ability.secondary_weapon_id = ITEM_ID_NONE;
    }

    if ((g_action_current_ability.primary_weapon_id == ITEM_ID_NONE
            && g_action_current_ability.secondary_weapon_id == ITEM_ID_NONE)
        || (g_action_current_ability.primary_weapon_id != ITEM_ID_NONE
            && g_action_current_ability.secondary_weapon_id != ITEM_ID_NONE)) {
        if (support & 1) {
            g_action_current_ability.strike_count = 2;
        }
    }
}

/*
 * Pick a random tile among those whose MAP_TILE_FLAG_TARGETED is set,
 * clearing the flag on every candidate and setting it again on the chosen
 * tile, then store that tile as the current action target.
 *
 * The candidate list reuses the targeting panels' mark byte for the tile
 * index, so upper-level candidates are recognized by their list position.
 */
void target_select_random_tile_for_random_fire_abilities(void) {
    s32 count;
    s32 lower_count;
    s32 i;
    s32 index;
    s32 tile;
    map_tile_marks_t marks;
    map_tile_t* tile_data;
    target_panel_t* panel;

    count = 0;
    for (i = 0; i < 0x200; i++) {
        tile_data = &g_map_tile_data[i];
        if (i == 0x100) {
            lower_count = count;
        }
        marks = tile_data->ceiling_depth_and_marks;
        if (marks.targeted) {
            tile_data->ceiling_depth_and_marks.targeted = 0;
            panel = &g_target_panels[count];
            count++;
            panel->mark = i;
        }
    }
    index = rand() * count / 32768;
    tile = g_target_panels[index].mark;
    if (index >= lower_count) {
        tile += 0x100;
    }
    g_map_tile_data[tile].ceiling_depth_and_marks.targeted = 1;
    g_action_current_ability.random_fire_flag = 1;
    g_action_current_ability.target_new_elevation = tile / 256;
    g_action_current_ability.target_new_y = (tile & 0xFF) / g_map_max_x;
    g_action_current_ability.target_new_x = (tile & 0xFF) % g_map_max_x;
    target_clear_panel_spread_flags();
}

/* Fill `list` (BATTLE_UNIT_SLOT_COUNT bytes, 0xff padded) with the ids of present units whose
 * tile carries `MAP_TILE_FLAG_TARGETED`, and return the count.
 * target_is_unit_untargetable_and_store_tile returns nonzero for units
 * that cannot be counted and writes the unit's tile index. When nearest-first
 * ordering is enabled the list is insertion-sorted by Manhattan distance from
 * `origin`.
 */
s32 target_list_units_on_panels(u8* list, const unit_stats_t* origin) {
    /* A byte flag test retains the original compound-condition scheduling. */
    s32 distance[BATTLE_UNIT_SLOT_COUNT];
    s32 tile;
    s32 count;
    s32 i;
    s32 j;
    s32 k;
    s32 dx;
    s32 dy;
    s32 dist;
    s32 x;
    s32 y;
    unit_stats_t* unit;

    count = 0;
    x = origin->x;
    y = origin->position.y;
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        unit = &g_battle_unit_stats[i];
        if (unit->entd_slot != BATTLE_ENTD_SLOT_NONE && target_is_unit_untargetable_and_store_tile(i, &tile) == 0
            && ((*(u8*)&g_map_tile_data[tile].ceiling_depth_and_marks) & MAP_TILE_FLAG_TARGETED)) {
            if (g_battle_sort_targets_nearest_first != 0) {
                dx = x - unit->x;
                if (dx < 0) {
                    dx = -dx;
                }
                dy = y - unit->position.y;
                if (dy < 0) {
                    dy = -dy;
                }
                dist = dx + dy;
                for (j = 0; j < count; j++) {
                    if (dist < distance[j]) {
                        break;
                    }
                }
                for (k = BATTLE_UNIT_SLOT_COUNT - 1; j < k; k--) {
                    list[k] = list[k - 1];
                    distance[k] = distance[k - 1];
                }
                distance[j] = dist;
            } else {
                j = count;
            }
            list[j] = i;
            count++;
        }
    }
    for (j = count; j < BATTLE_UNIT_SLOT_COUNT; j++) {
        list[j] = 0xFF;
    }
    return count;
}

/*
 * Check action restrictions and consume MP outside preview mode.
 *
 * Return 0 when allowed, or a rejection code. Only ordinary-menu,
 * non-Mime abilities consume MP; the effective cost is recorded first.
 * Preview skips only subtraction; earlier bookkeeping still runs.
 */
s32 action_check_and_consume_mp(unit_stats_t* unit) {
    s16 ability_id;
    u8 menu;
    s32 result;
    u8 cost;
    s16 mp;
    ability_secondary_data_t* ability;

    ability_id = unit->action_command.ability_id;
    menu = g_main_action_menu_types_by_skillset[unit->action_command.skillset_id];
    g_action_current_ability.mp_cost = 0;
    if (g_action_context == ACTION_CONTEXT_PRIMARY)
        main_status_set_action_state(unit, MAIN_UNIT_ACTION_STATE_KEEP_PERFORMING);
    result = battle_unit_get_action_restriction(unit);
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DONT_ACT)] & STATUS_BYTE_MASK(STATUS_ID_DONT_ACT))
        result = BATTLE_UNIT_ACTION_RESTRICTION_CANNOT_ACT;
    if (result)
        return result + 6;
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FROG)] & STATUS_BYTE_MASK(STATUS_ID_FROG)) {
        if (ability_id != 0 && ability_id != ABILITY_ID_FROG_ATTACK && ability_id != ABILITY_ID_BLACK_MAGIC_FROG)
            return 5;
        if (menu == ACTION_MENU_TYPE_CHARGE)
            return 5;
    }
    if (ability_id >= 0 && ability_id < ABILITY_ID_ITEM_FIRST) {
        ability = &g_main_ability_range_data[ability_id];
        if ((menu == ACTION_MENU_TYPE_DEFAULT || menu == ACTION_MENU_TYPE_MONSTER)
            && (ability->flags_3 & ABILITY_SECONDARY_FLAG_3_AFFECTED_BY_SILENCE)
            && (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_SILENCE)] & STATUS_BYTE_MASK(STATUS_ID_SILENCE)))
            return 2;
        if (menu == ACTION_MENU_TYPE_DEFAULT && unit->job_id != JOB_ID_MIME) {
            cost = ability->mp_cost;
            if (unit->ability_flags.support[1] & BATTLE_SUPPORT_SET_2_HALF_MP)
                cost >>= 1;
            g_action_current_ability.mp_cost = cost;
            /* A signed member view retains the target store-before-load order;
             * casting the unsigned value hoists the MP load and loses a nop. */
            mp = unit->mp;
            if (mp < cost)
                return 3;
            if (g_action_state != ACTION_STATE_PREVIEW)
                unit->mp = mp - cost;
        }
    }
    return 0;
}

/*
 * Save the actor's position or restore its pre-reaction action command.
 *
 * A Mime's weapons are cleared first. Normal actions save X/Y, elevation,
 * and facing; reactions restore the 20 saved command bytes at 0x16e.
 */
void action_store_acting_unit_data(unit_stats_t* unit) {
    if (unit->job_id == JOB_ID_MIME) {
        unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] = ITEM_ID_NONE; /* right hand */
        unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON] = ITEM_ID_NONE;  /* left hand */
    } else if (g_action_context == ACTION_CONTEXT_PRIMARY) {
        g_action_current_attacker.facing = unit->position.facing;
    }

    if (g_action_context == ACTION_CONTEXT_PRIMARY) {
        g_battle_acting_unit_x = unit->x;
        g_battle_acting_unit_y = unit->position.y;
        g_battle_acting_unit_elevation = unit->position.higher_elevation;
    } else {
        main_util_copy_action_data(&g_action_saved_command, &unit->action_command);
        g_action_context = ACTION_CONTEXT_PRIMARY;
    }
}

/* The unit on a tile: battle_unit_find_at_tile with no status filter. */
s32 target_find_unit_at_tile(s32 x, s32 y, s32 level) {
    return battle_unit_find_at_tile(x, y, level, 0);
}

/*
 * Find an eligible unit occupying the requested map tile.
 *
 * The status filter bits are alternatives: a unit with any requested status
 * is eligible. The Chocobo bit is an additional requirement. Mounted riders
 * resolve to the mount's battle slot, and invalid coordinates or terrain are
 * distinguished from an empty tile.
 */
s32 battle_unit_find_at_tile(s32 x, s32 y, u32 level, s32 filters) {
    /* A byte collision test retains the original single-mask branch. */
    s32 unit_index;
    unit_stats_t* unit;
    map_tile_t* tile;
    u8 mount_info;
    /* The unit loop tests a byte copy of the filters. */
    u8 unit_filters;

    unit_filters = filters;

    if (x < 0 || x >= g_map_max_x || y < 0 || y >= g_map_max_y || level >= 2) {
        return -2;
    }

    tile = &g_map_tile_data[(level << 8) + y * g_map_max_x + x];
    if (((*(u8*)&tile->flags) & MAP_TILE_COLLISION_MASK)) {
        return -2;
    }
    if (filters & BATTLE_UNIT_TILE_FILTER_DRY_LOW_SLOPE) {
        if (tile->depth_half_height.depth != 0) {
            return -2;
        }
        if (tile->depth_half_height.half_height >= 3) {
            return -2;
        }
    }

    for (unit_index = 0; unit_index < BATTLE_UNIT_SLOT_COUNT; unit_index++) {
        unit = &g_battle_unit_stats[unit_index];
        if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
            continue;
        }
        if ((unit_filters & BATTLE_UNIT_TILE_STATUS_FILTERS)
            && !((unit_filters & BATTLE_UNIT_TILE_FILTER_CRYSTAL)
                && (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)]
                    & STATUS_BYTE_MASK(STATUS_ID_CRYSTAL)))
            && !((unit_filters & BATTLE_UNIT_TILE_FILTER_TREASURE)
                && (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_TREASURE)]
                    & STATUS_BYTE_MASK(STATUS_ID_TREASURE)))
            && !((unit_filters & BATTLE_UNIT_TILE_FILTER_TRANSPARENT)
                && (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_TRANSPARENT)]
                    & STATUS_BYTE_MASK(STATUS_ID_TRANSPARENT)))) {
            continue;
        }
        if ((unit_filters & BATTLE_UNIT_TILE_FILTER_MOUNTABLE_CHOCOBO)
            && battle_unit_get_chocobo_mount_state(unit) == BATTLE_CHOCOBO_MOUNT_NONE) {
            continue;
        }
        if (unit->x != x || unit->position.y != y || unit->position.higher_elevation != level) {
            continue;
        }

        mount_info = unit->mount_info;
        if (mount_info >= BATTLE_MOUNT_INFO_FLAG_RIDER) {
            return mount_info & BATTLE_MOUNT_INFO_PARTNER_ID_MASK;
        }
        return unit_index;
    }

    return -1;
}

/* Clear the range and the mark of all 512 targeting panels. */
void target_clear_panel_data(void) {
    s32 i = 0;
    target_panel_t* panel = g_target_panel_data;
    for (; i < 512; i++) {
        panel->remaining_range = 0;
        panel->mark = 0;
        panel++;
    }
}

/*
 * Give the single unit targeted by `id`'s pending action a chance to Hamedo
 * the actor, reporting the reaction ability through `out_ability`.
 *
 * Returns the reacting unit's slot, or -1 when nothing reacts.
 *
 * target_validate_weapon_target takes only the action record (it copies the record and
 * rederives the menu type itself); menu_type and flags_1 merely happen to sit
 * in $a1/$a2 at that call. Passing them as arguments would set $a2 twice and
 * change the scheduling of the counter-ability call's argument setup.
 */
s32 battle_reaction_prepare_hamedo_for_pending_action(s32 id, s16* out_ability) {
    u8 targets[BATTLE_UNIT_SLOT_COUNT];
    unit_stats_t* unit;
    unit_stats_t* base;
    unit_stats_t* target;
    action_command_t* action;
    s32 i;
    s32 count;
    s32 result;
    s32 menu_type;
    u8 flags_1;
    u16 mp;
    s32 tid;
    u16 ability_id;

    unit = &g_battle_unit_stats[id];
    base = g_battle_unit_stats;
    g_action_hamedo_flag = 0;
    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
        return -1;
    }
    mp = unit->mp;
    result = action_check_and_consume_mp(unit);
    unit->mp = mp;
    if (result != 0) {
        return -1;
    }

    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        base[i].action.reaction_id = 0;
        base[i].ability_outcome = 0;
    }

    menu_type = g_main_action_menu_types_by_skillset[unit->action_command.skillset_id];
    flags_1 = 0;
    if ((menu_type & 0xff) == ACTION_MENU_TYPE_DEFAULT || (menu_type & 0xff) == ACTION_MENU_TYPE_MONSTER) {
        ability_id = unit->action_command.ability_id;
        if (ability_id < ABILITY_ID_ITEM_FIRST) {
            flags_1 = g_main_ability_range_data[ability_id].flags_1;
        }
    }

    result = -1;
    if ((menu_type & 0xff) == ACTION_MENU_TYPE_ATTACK || (menu_type & 0xff) == ACTION_MENU_TYPE_CHARGE
        || (menu_type & 0xff) == ACTION_MENU_TYPE_WEAPON_INVENTORY
        || (flags_1 & ABILITY_SECONDARY_FLAG_1_WEAPON_RANGE)) {
        result = target_validate_weapon_target(&unit->action_command);
    }
    if (result == -1) {
        return -1;
    }

    g_battle_sort_targets_nearest_first = 0;
    count = target_list_units_on_panels(targets, unit);
    if (count != 1) {
        return -1;
    }
    tid = targets[0];
    if (tid == id) {
        return -1;
    }
    target = &g_battle_unit_stats[tid];
    if (!(target->ability_flags.reaction[3] & BATTLE_REACTION_SET_4_HAMEDO)) {
        return -1;
    }
    if (action_calculate_chance_to_react(target) != 0) {
        return -1;
    }
    action = &target->action_command;
    if (action_is_reaction_prevented(target) != 0) {
        return -1;
    }

    g_action_context = count;
    main_util_copy_action_data(action, &g_action_saved_command);
    g_battle_current_reaction_ability_id = ABILITY_ID_REACTION_HAMEDO;
    g_battle_acting_unit_id = id;
    target->action_command.actor_id = tid;
    if (action_store_counter_ability(target, SKILLSET_ID_ATTACK, 0, 1) == 0
        && action_check_and_consume_mp(target) == 0) {
        action_init_current_ability_strike_data(target);
        *out_ability = g_battle_current_reaction_ability_id;
        g_action_hamedo_flag = count;
        if (unit->job_id == JOB_ID_MIME) {
            unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] = ITEM_ID_NONE;
            unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON] = ITEM_ID_NONE;
        }
        return tid;
    }
    main_util_copy_action_data(&g_action_saved_command, action);
    return -1;
}

/* Reaction-ability outcome carried in `outcome` between the per-ability arms
 * and the common tail. */
enum {
    BATTLE_REACTION_OUTCOME_NONE = 0,
    BATTLE_REACTION_OUTCOME_COUNTER = 2,
};

/* Finds the first unit holding a pending reaction and resolves it.
 *
 * Walks the 21 unit slots, skipping the acting unit, and for the first unit
 * whose queued reaction id is set and who can still react, saves the unit's
 * action block, rebuilds it for the reaction and runs the matching preparation.
 * Returns the reacting unit's slot and writes its reaction ability id to
 * `out_ability`, or -1 when no unit reacts.
 *
 * The action pointer has its own register because stores use that base while
 * the unit pointer can change for Damage Split.
 */
s32 battle_reaction_prepare_next(s16* out_ability) {
    unit_stats_t* unit;
    action_command_t* action;
    s32 i;
    s32 outcome;
    s32 result;
    s32 reaction_prevented;
    u16 ability;
    u16 last_attack;
    s16 reaction_id;

    if (g_battle_acting_unit_id == -1) {
        return -1;
    }
    i = 0;
    do {
        g_action_context = ACTION_CONTEXT_PRIMARY;
        if (i != g_battle_acting_unit_id) {
            unit = &g_battle_unit_stats[i];
            action = &unit->action_command;
            if (battle_unit_get_action_restriction(unit) == BATTLE_UNIT_ACTION_RESTRICTION_NONE
                && unit->entd_slot != BATTLE_ENTD_SLOT_NONE) {
                reaction_prevented = action_is_reaction_prevented(unit);
                ability = unit->action.reaction_id;
                if (((reaction_prevented == 0 && g_action_state != ACTION_STATE_AI_SIMULATION)
                        || ability == ABILITY_ID_REACTION_REFLECT)
                    && ability != 0) {
                    g_action_context = ACTION_CONTEXT_REACTION_OR_SIMULATION;
                    main_util_copy_action_data(action, &g_action_saved_command);
                    g_battle_current_reaction_ability_id = ability;
                    action->actor_id = i;
                    action->skillset_id = 0;
                    reaction_id = g_battle_current_reaction_ability_id;
                    outcome = 1;
                    action->ability_id = reaction_id;
                    last_attack = unit->action.last_received_attack;
                    unit->action.reaction_id = 0;
                    switch (reaction_id) {
                    case ABILITY_ID_REACTION_REFLECT:
                        outcome = -(target_apply_reflect(unit) == 0) & 3;
                        break;
                    case ABILITY_ID_REACTION_MEATBONE_SLASH:
                        if (action_store_counter_ability(unit, SKILLSET_ID_ATTACK, 0, 1) != 0) {
                            outcome = BATTLE_REACTION_OUTCOME_NONE;
                        } else {
                            action->skillset_id = 1;
                            action->targeting_type = 5;
                        }
                        break;
                    case ABILITY_ID_REACTION_COUNTER_MAGIC:
                        outcome
                            = (action_store_counter_ability(unit, SKILLSET_ID_BLACK_MAGIC, last_attack, 0) == 0) * 2;
                        break;
                    case ABILITY_ID_REACTION_COUNTER_TACKLE:
                        if (action_store_counter_ability(unit, SKILLSET_ID_BLACK_MAGIC, ABILITY_ID_BASIC_SKILL_DASH, 1)
                            != 0) {
                            outcome = BATTLE_REACTION_OUTCOME_NONE;
                        }
                        break;
                    case ABILITY_ID_REACTION_COUNTER_FLOOD:
                        outcome = (action_store_counter_ability(unit, SKILLSET_ID_ELEMENTAL,
                                       g_main_terrain_geomancy_abilities[g_map_tile_data[map_calculate_location(unit)]
                                               .surface.type],
                                       0)
                                      == 0)
                            * 2;
                        break;
                    case ABILITY_ID_REACTION_AUTO_POTION:
                        result = action_select_auto_potion_item(unit);
                        if (result == -1) {
                            outcome = BATTLE_REACTION_OUTCOME_NONE;
                            break;
                        }
                        action->skillset_id = 6;
                        /* Auto-Potion updates only the low byte of the 16-bit item/equipment field. */
                        *(u8*)&action->item_or_equipment = result;
                        action->targeting_type = 5;
                        action->target_id = i;
                        action->target_position.vx = unit->x;
                        action->target_position.vz = unit->position.y;
                        action->target_position.vy = unit->position.higher_elevation;
                        result = action_call_attack_preparation_at_preview(action);
                        outcome = BATTLE_REACTION_OUTCOME_COUNTER;
                        unit->action_command.ability_id = ABILITY_ID_REACTION_AUTO_POTION;
                        if (result == -1 || result == 2 || result == 3) {
                            outcome = BATTLE_REACTION_OUTCOME_NONE;
                        }
                        break;
                    case ABILITY_ID_REACTION_COUNTER:
                        if (action_store_counter_ability(unit, SKILLSET_ID_ATTACK, 0, 1) != 0) {
                            outcome = BATTLE_REACTION_OUTCOME_NONE;
                        }
                        break;
                    default:
                        outcome
                            = -((g_main_reaction_behavior_flags[g_battle_current_reaction_ability_id
                                     - ABILITY_ID_REACTION_FIRST]
                                    & (BATTLE_REACTION_BEHAVIOR_FLAG_PASSIVE | BATTLE_REACTION_BEHAVIOR_FLAG_ACTIVE))
                                == 0);
                        break;
                    }
                    if ((outcome == 1 || outcome == 2) && action_check_and_consume_mp(unit) != 0) {
                        outcome = BATTLE_REACTION_OUTCOME_NONE;
                    }
                    if (outcome == BATTLE_REACTION_OUTCOME_NONE) {
                        main_util_copy_action_data(&g_action_saved_command, action);
                        i++;
                        continue;
                    }
                    if (outcome >= 1 && outcome <= 3) {
                        action_init_current_ability_strike_data(unit);
                    } else {
                        g_action_current_ability.strike_count = 1;
                        g_action_current_ability.strike_counter = 0;
                        g_action_current_ability.weapon_proc_pending = 0;
                        g_action_current_ability.knockback_flags = 0;
                        g_action_current_ability.primary_weapon_id
                            = unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON];
                        g_action_current_ability.secondary_weapon_id
                            = unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON];
                        action->skillset_id = 0;
                        action->targeting_type = 5;
                        action->target_id = i;
                        if (g_battle_current_reaction_ability_id == ABILITY_ID_REACTION_DAMAGE_SPLIT) {
                            action->target_id = g_battle_acting_unit_id;
                            unit = &g_battle_unit_stats[g_battle_acting_unit_id];
                        }
                        action->target_position.vx = unit->x;
                        action->target_position.vz = unit->position.y;
                        action->target_position.vy = unit->position.higher_elevation;
                    }
                    *out_ability = g_battle_current_reaction_ability_id;
                    action->actor_id = i;
                    return i;
                }
            }
        }
        i++;
    } while (i < 21);
    g_action_context = ACTION_CONTEXT_PRIMARY;
    *out_ability = 0;
    return -1;
}

/* Aim a reflected spell: the unit casts the ability it received, under the
 * Black Magic skillset, at the tile as far beyond it as the caster is in front
 * of it. -1 when the caster stands on the unit's tile or that tile is off the
 * map or blocked. */
s32 target_apply_reflect(unit_stats_t* unit) {
    action_command_t* act;
    s32 x;
    s32 y;
    s32 z;
    s32 dx;
    s32 dy;
    s32 last_attack;

    act = &unit->action_command;
    dx = unit->x - g_battle_acting_unit_x;
    dy = unit->position.y - g_battle_acting_unit_y;
    if (dx == 0 && dy == 0) {
        return -1;
    }
    if (map_resolve_rotated_offset_layer(unit, dx, dy, 0, &x, &y, &z) != 0) {
        return -1;
    }
    act->skillset_id = SKILLSET_ID_BLACK_MAGIC;
    last_attack = unit->action.last_received_attack;
    act->targeting_type = BATTLE_TARGETING_TYPE_TILE;
    act->target_id = 0;
    act->ability_id = last_attack;
    act->target_position.vx = x;
    act->target_position.vz = y;
    act->target_position.vy = z;
    return 0;
}
