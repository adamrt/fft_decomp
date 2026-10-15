/* Attack preparation, including height data, target maps, MP cost, reaction setup, ENTD unit loading and placement
 * validation. */

#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_effect.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_move.h"
#include "fft/battle_script.h"
#include "fft/battle_status.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "fft/main.h"
#include "fft/script.h"
#include "psx/types.h"

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
        tile = &g_map_tile_data[battle_map_calculate_location(unit)];
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
void battle_action_call_attack_preparation(action_command_t* action) {
    battle_action_prepare_attack(action, action, 0);
}

/* Run action_prepare_attack phase 1 on a command, writing the prepared
 * action into the acting unit's own action fields.
 *
 * An action that resolves immediately also ends the unit's Charging, Jump,
 * Defending and Performing statuses, but only in the primary action context.
 * Returns the battle_attack_preparation_e result. */
s32 battle_action_call_attack_preparation_at_preview(action_command_t* command) {
    unit_stats_t* unit;
    s32 result;

    unit = &g_battle_unit_stats[command->actor_id];
    result = battle_action_prepare_attack(command, &unit->action_command, 1);
    if (result == BATTLE_ATTACK_PREPARATION_IMMEDIATE && g_action_context == ACTION_CONTEXT_PRIMARY) {
        battle_status_enable_disable_acting(unit);
    }
    return result;
}

/*
 * The one-trip loop around `skip = 1` counts that set one loop level deeper,
 * so global-alloc ranks `skip` above `persevere` and gives them s6/s7 as in
 * the target.
 */
s32 battle_action_prepare_attack(action_command_t* source, action_command_t* dest, s32 phase) {
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
            battle_action_decrement_player_item_quantity(unit, item, 1);
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
            battle_action_decrement_player_item_quantity(unit, item, 1);
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
        action.ability_id = battle_action_get_elemental_ability_id(unit);
        break;
    case ACTION_MENU_TYPE_KATANA_INVENTORY:
        item = action.item_or_equipment;
        if (item < ITEM_ID_ASURA_KNIFE || item > ITEM_ID_CHIRIJIRADEN) {
            return BATTLE_ATTACK_PREPARATION_INVALID;
        }
        ability = item + (ABILITY_ID_DRAW_OUT_ASURA - ITEM_ID_ASURA_KNIFE);
        action.ability_id = ability;
        if (phase & 1) {
            battle_action_decrement_player_item_quantity(unit, item, 1);
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
s32 battle_action_decrement_player_item_quantity(unit_stats_t* unit, s32 item_id, s32 always_one) {
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
s32 battle_action_init_current_data(s32 id) {
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
        battle_action_store_acting_unit_data(unit);
        return 6;
    }
    mp_result = battle_action_check_and_consume_mp(unit);
    if (mp_result != 0) {
        battle_action_store_acting_unit_data(unit);
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
    battle_action_init_current_ability_strike_data(unit);
    battle_action_set_current_attacker_data(unit);
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
s32 battle_action_build_reaction_targets(s32 actor_id, battle_strike_work_t* work, s32* target_count, u8* target_ids) {
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
        target_or_count = battle_target_find_unit_at_tile(target_x, target_y, target_elevation);
        if (target_or_count < 0) {
            *target_count = 0;
            break;
        }
        unit = &unit_base[target_or_count];
        mount_info = unit->mount_info;
        if (mount_info & BATTLE_MOUNT_INFO_FLAG_MOUNT) {
            target_or_count = mount_info & BATTLE_MOUNT_INFO_PARTNER_ID_MASK;
        }
        if (battle_target_is_unit_untargetable_and_store_tile(target_or_count, &tile_index) != 0) {
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
s32 battle_target_calculate_map_for_action(const action_command_t* action, u8* flags_3) {
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
        result = battle_target_validate_weapon_target(action);
    } else {
        result = battle_target_calculate_for_menu_types(action);
        if (result != -1 && (flags_2 & ABILITY_SECONDARY_FLAG_2_RANDOM_FIRE)
            && g_action_state == ACTION_STATE_EXECUTE) {
            battle_target_select_random_tile_for_random_fire_abilities();
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
s32 battle_action_resolve_ability_strike(s32 battle_id, battle_strike_work_t* work) {
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
    battle_action_clear_current_data(&attacker->action);
    attacker->action.hit = 0;
    attacker->action.miss_type = ACTION_MISS_TYPE_CANCELED;
    battle_action_clear_current_data(&g_action_attacker_action_data);
    g_action_attacker_action_data.hit = 0;
    work->knockback_flags = 0;
    work->post_action_message = BATTLE_POST_ACTION_MESSAGE_NONE;
    work->math_skill_flag = 0;
    work->reaction_ability_id = 0;
    reaction_targeted = 0;
    if (g_action_context != ACTION_CONTEXT_PRIMARY) {
        work->reaction_ability_id = g_battle_current_reaction_ability_id;
        if (battle_action_build_reaction_targets(battle_id, work, &count, targets)) {
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
        if (battle_target_calculate_map_for_action(&attacker->action_command, &flags_3) == -1) {
            work->target_count = 0;
            work->continue_attack = 0;
            return -1;
        }
        if (g_action_current_ability.post_action_target_id < BATTLE_UNIT_SLOT_COUNT
            && (g_action_current_ability.weapon_proc_pending != 0
                || (g_action_current_ability.knockback_flags & BATTLE_KNOCKBACK_PENDING))) {
            battle_target_disable_green_panel_flags();
            count = 1;
            targets[0] = g_action_current_ability.post_action_target_id;
            for (i = 1; i < BATTLE_UNIT_RUNTIME_SLOT_COUNT; i++) {
                targets[i] = 0xff;
            }
            tile = &g_map_tile_data[battle_map_calculate_location(
                &g_battle_unit_stats[g_action_current_ability.post_action_target_id])];
            tile->ceiling_depth_and_marks.targeted = 1;
        }
        count = battle_target_list_units_on_panels(targets, attacker);
    }
    g_action_current_ability.target_count = count;
    battle_action_store_target_ids(targets);
    battle_action_store_used_weapon(&attacker->action_command);
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        target_id = targets[i];
        if (target_id != 0xff) {
            g_action_current_ability.defaulted_to_attack = 0;
            battle_action_run_pre_formula_setup(&attacker->action_command, target_id);
        }
    }
    battle_status_remove_transparent_if_jump_used(attacker);
    count = battle_target_copy_ability_targets(work->target_list);
    work->target_count = count;
    g_action_current_ability.target_count = count;
    if (g_action_context == ACTION_CONTEXT_PRIMARY) {
        g_action_current_ability.can_earn_exp_jp = 1;
    }
    work->can_earn_experience = 0;
    battle_action_finalize_draw_out_katana_result(attacker, work, count);
    work->actor_id = battle_id;
    work->used_weapon_id = g_action_current_ability.weapon_id;
    work->reaction_occurred = 0;
    work->current_hit_number = g_action_current_ability.strike_counter;
    battle_target_set_coordinates_for_ability(attacker, work);
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
void battle_target_set_coordinates_for_ability(const unit_stats_t* acting, battle_strike_work_t* out) {
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
void battle_action_finalize_draw_out_katana_result(unit_stats_t* attacker, battle_strike_work_t* work, u8 hit_count) {
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
void battle_action_init_current_ability_strike_data(unit_stats_t* unit) {
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
void battle_target_select_random_tile_for_random_fire_abilities(void) {
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
    battle_target_clear_panel_spread_flags();
}

/* Fill `list` (BATTLE_UNIT_SLOT_COUNT bytes, 0xff padded) with the ids of present units whose
 * tile carries `MAP_TILE_FLAG_TARGETED`, and return the count.
 * target_is_unit_untargetable_and_store_tile returns nonzero for units
 * that cannot be counted and writes the unit's tile index. When nearest-first
 * ordering is enabled the list is insertion-sorted by Manhattan distance from
 * `origin`.
 */
s32 battle_target_list_units_on_panels(u8* list, const unit_stats_t* origin) {
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
        if (unit->entd_slot != BATTLE_ENTD_SLOT_NONE && battle_target_is_unit_untargetable_and_store_tile(i, &tile) == 0
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
s32 battle_action_check_and_consume_mp(unit_stats_t* unit) {
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
void battle_action_store_acting_unit_data(unit_stats_t* unit) {
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
s32 battle_target_find_unit_at_tile(s32 x, s32 y, s32 level) {
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
void battle_target_clear_panel_data(void) {
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
    result = battle_action_check_and_consume_mp(unit);
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
        result = battle_target_validate_weapon_target(&unit->action_command);
    }
    if (result == -1) {
        return -1;
    }

    g_battle_sort_targets_nearest_first = 0;
    count = battle_target_list_units_on_panels(targets, unit);
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
    if (battle_action_calculate_chance_to_react(target) != 0) {
        return -1;
    }
    action = &target->action_command;
    if (battle_action_is_reaction_prevented(target) != 0) {
        return -1;
    }

    g_action_context = count;
    main_util_copy_action_data(action, &g_action_saved_command);
    g_battle_current_reaction_ability_id = ABILITY_ID_REACTION_HAMEDO;
    g_battle_acting_unit_id = id;
    target->action_command.actor_id = tid;
    if (battle_action_store_counter_ability(target, SKILLSET_ID_ATTACK, 0, 1) == 0
        && battle_action_check_and_consume_mp(target) == 0) {
        battle_action_init_current_ability_strike_data(target);
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
                reaction_prevented = battle_action_is_reaction_prevented(unit);
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
                        outcome = -(battle_target_apply_reflect(unit) == 0) & 3;
                        break;
                    case ABILITY_ID_REACTION_MEATBONE_SLASH:
                        if (battle_action_store_counter_ability(unit, SKILLSET_ID_ATTACK, 0, 1) != 0) {
                            outcome = BATTLE_REACTION_OUTCOME_NONE;
                        } else {
                            action->skillset_id = 1;
                            action->targeting_type = 5;
                        }
                        break;
                    case ABILITY_ID_REACTION_COUNTER_MAGIC:
                        outcome
                            = (battle_action_store_counter_ability(unit, SKILLSET_ID_BLACK_MAGIC, last_attack, 0) == 0)
                            * 2;
                        break;
                    case ABILITY_ID_REACTION_COUNTER_TACKLE:
                        if (battle_action_store_counter_ability(
                                unit, SKILLSET_ID_BLACK_MAGIC, ABILITY_ID_BASIC_SKILL_DASH, 1)
                            != 0) {
                            outcome = BATTLE_REACTION_OUTCOME_NONE;
                        }
                        break;
                    case ABILITY_ID_REACTION_COUNTER_FLOOD:
                        outcome = (battle_action_store_counter_ability(unit, SKILLSET_ID_ELEMENTAL,
                                       g_main_terrain_geomancy_abilities
                                           [g_map_tile_data[battle_map_calculate_location(unit)].surface.type],
                                       0)
                                      == 0)
                            * 2;
                        break;
                    case ABILITY_ID_REACTION_AUTO_POTION:
                        result = battle_action_select_auto_potion_item(unit);
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
                        result = battle_action_call_attack_preparation_at_preview(action);
                        outcome = BATTLE_REACTION_OUTCOME_COUNTER;
                        unit->action_command.ability_id = ABILITY_ID_REACTION_AUTO_POTION;
                        if (result == -1 || result == 2 || result == 3) {
                            outcome = BATTLE_REACTION_OUTCOME_NONE;
                        }
                        break;
                    case ABILITY_ID_REACTION_COUNTER:
                        if (battle_action_store_counter_ability(unit, SKILLSET_ID_ATTACK, 0, 1) != 0) {
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
                    if ((outcome == 1 || outcome == 2) && battle_action_check_and_consume_mp(unit) != 0) {
                        outcome = BATTLE_REACTION_OUTCOME_NONE;
                    }
                    if (outcome == BATTLE_REACTION_OUTCOME_NONE) {
                        main_util_copy_action_data(&g_action_saved_command, action);
                        i++;
                        continue;
                    }
                    if (outcome >= 1 && outcome <= 3) {
                        battle_action_init_current_ability_strike_data(unit);
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
s32 battle_target_apply_reflect(unit_stats_t* unit) {
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
    if (battle_map_resolve_rotated_offset_layer(unit, dx, dy, 0, &x, &y, &z) != 0) {
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

/* Writes the acting unit's tile into unit's action fields (0x16e..) as the
 * target of skillset/ability and runs the attack preview.  Returns -1 when the
 * action itself fails, -2 when the acting unit's tile is not targetable and
 * -3 when the preview fails; 0 otherwise. */
s32 battle_action_store_counter_ability(unit_stats_t* unit, s8 skillset_id, s16 ability_id, s32 validate_target) {
    /* A byte flag test retains the original compound-condition scheduling. */
    action_command_t* action;
    unit_stats_t* acting_unit;
    s32 result;
    s32 acting_unit_id;
    s32 preview_result;
    u8 saved_charged_ability_ct;

    acting_unit_id = g_battle_acting_unit_id;
    action = &unit->action_command;
    unit->action_command.skillset_id = skillset_id;
    unit->action_command.ability_id = ability_id;
    acting_unit = &g_battle_unit_stats[acting_unit_id];
    if (validate_target != 0) {
        result = battle_target_set_panels_for_action(action);
        if (result == -1 || result == 2 || result == 3) {
            return -1;
        }
        if (((*(u8*)&g_map_tile_data[battle_map_calculate_location(acting_unit)].ceiling_depth_and_marks)
                & MAP_TILE_FLAG_ABILITY_RANGE)
            == 0) {
            return -2;
        }
    }
    action->targeting_type = BATTLE_TARGETING_TYPE_TILE;
    action->target_id = g_battle_acting_unit_id;
    action->target_position.vx = acting_unit->x;
    action->target_position.vz = acting_unit->position.y;
    action->target_position.vy = acting_unit->position.higher_elevation;
    saved_charged_ability_ct = unit->charged_ability_ct;
    result = battle_action_call_attack_preparation_at_preview(action);
    unit->charged_ability_ct = saved_charged_ability_ct;
    if (result == BATTLE_ATTACK_PREPARATION_INVALID
        || (preview_result
            = (result == BATTLE_ATTACK_PREPARATION_CHANGE_EQUIPMENT || result == BATTLE_ATTACK_PREPARATION_JUMP),
            preview_result != 0)) {
        preview_result = -3;
    }
    return preview_result;
}

/* The item Auto Potion uses: the first of Potion, Hi-Potion and X-Potion the
 * party has in stock, or -1 with none. A unit that did not start on the blue team
 * always uses a Potion. */
s32 battle_action_select_auto_potion_item(unit_stats_t* unit) {
    if (unit->initial_team_flags & BATTLE_TEAM_MASK) {
        return ITEM_ID_POTION;
    }
    if (g_main_item_quantities[ITEM_ID_POTION] != 0) {
        return ITEM_ID_POTION;
    }
    if (g_main_item_quantities[ITEM_ID_HI_POTION] != 0) {
        return ITEM_ID_HI_POTION;
    }
    if (g_main_item_quantities[ITEM_ID_X_POTION] != 0) {
        return ITEM_ID_X_POTION;
    }
    return -1;
}

s32 battle_unit_apply_action_rewards(s32 unit_index, action_reward_display_t* display) {
    s32 job_slot;
    unit_stats_t* unit;
    u8 old_level;
    s32 experience;
    s32 jp;
    s32 unit_id;
    u8 team;
    u8 job_id;
    s32 old_job_level;
    s32 new_job_level;
    s32 job_component;
    s32 level_component;

    display->earned_experience = 0;
    display->earned_jp = 0;
    display->level_for_display = 0;
    display->job_level_for_display = 0;

    if (g_action_hamedo_flag != 0) {
        return -1;
    }
    if (g_action_current_ability.can_earn_exp_jp == 0) {
        return 0;
    }

    unit = &g_battle_unit_stats[unit_index];
    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
        return -1;
    }
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)]
        & (STATUS_BYTE_MASK(STATUS_ID_CRYSTAL) | STATUS_BYTE_MASK(STATUS_ID_DEAD))) {
        return -1;
    }
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_PETRIFY)]
        & (STATUS_BYTE_MASK(STATUS_ID_PETRIFY) | STATUS_BYTE_MASK(STATUS_ID_TREASURE))) {
        return -1;
    }

    experience = unit->experience + g_action_current_ability.earned_experience;
    old_level = unit->level;
    if (experience >= 0x100) {
        experience = 0xff;
    }
    unit->experience = experience;
    if (main_unit_check_level_up(unit) != 0 || g_action_current_ability.level_gained_flag != 0) {
        display->level_for_display = unit->level;
    }
    display->earned_experience = g_action_current_ability.earned_experience;

    if (unit->unit_flags & UNIT_FLAG_MONSTER) {
        return 0;
    }

    job_id = unit->job_id;
    old_job_level = battle_unit_load_job_level(unit, job_id, &job_slot);
    job_component = old_job_level * 2;
    level_component = (old_level >> 2) + 8;
    jp = job_component + level_component;
    if (g_action_current_ability.earned_experience == 0) {
        jp = 0;
    }

    team = unit->team_flags & BATTLE_TEAM_MASK;
    for (unit_id = 0; unit_id < BATTLE_UNIT_SLOT_COUNT; unit_id++) {
        if (unit_id != unit_index) {
            battle_unit_store_jp_and_calculate_unlocked_jobs(unit_id, jp / 4, team, job_slot);
        }
    }

    if (unit->ability_flags.support[1] & BATTLE_SUPPORT_SET_2_GAINED_JP_UP) {
        jp = jp * 3 / 2;
    }
    team = unit->initial_team_flags & BATTLE_TEAM_MASK;
    battle_unit_store_jp_and_calculate_unlocked_jobs(unit_index, jp, team, job_slot);

    new_job_level = battle_unit_load_job_level(unit, job_id, &job_slot);
    if (new_job_level != old_job_level) {
        display->job_level_for_display = new_job_level;
    }
    display->earned_jp = jp;
    return 0;
}

/*
 * Returns the unit's level (0..8) in the generic job job_id, from the
 * nibble-packed unit_stats_t.career.job_levels bytes at 0xd2.
 * *out_slot receives the zero-based job slot (Chemist = 1 ... Mime = 19),
 * or 0 for Squire and non-generic jobs.
 */
s32 battle_unit_load_job_level(unit_stats_t* unit, s32 job_id, s32* out_slot) {
    s32 slot;
    s32 packed;
    if (job_id >= JOB_ID_CHEMIST && job_id <= JOB_ID_MIME) {
        *out_slot = job_id - JOB_ID_SQUIRE;
    } else {
        *out_slot = 0;
    }
    slot = *out_slot;
    packed = unit->career.job_levels[slot / 2];
    /* Reassigning packed in both arms keeps the target's `sra`; returning
     * `packed >> 4` directly lets GCC use the byte's range and emit `srl`. */
    if (!(slot & 1)) {
        packed >>= 4;
    } else {
        packed &= 0xF;
    }
    return packed;
}

/* Adds JP to a battle unit's job slot and recomputes its level and unlocks.
 *
 * Skips absent, crystal, dead, treasure, monster and other-team units, and
 * the Bard slot for females and the Dancer slot for males. The battle twin of
 * main_job_add_proposition_jp; returns -1 when nothing is stored. */
s32 battle_unit_store_jp_and_calculate_unlocked_jobs(s32 unit_index, s32 jp, s32 team, s32 slot) {
    unit_stats_t* unit;
    s32 total;
    s32 level;
    s32 levels;
    u8 flags;
    u8 level_byte;

    if (jp < 0) {
        return -1;
    }
    unit = &g_battle_unit_stats[unit_index];
    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
        return -1;
    }
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)]
        & (STATUS_BYTE_MASK(STATUS_ID_CRYSTAL) | STATUS_BYTE_MASK(STATUS_ID_DEAD))) {
        return -1;
    }
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_TREASURE)] & STATUS_BYTE_MASK(STATUS_ID_TREASURE)) {
        return -1;
    }
    flags = unit->unit_flags;
    if (flags & UNIT_FLAG_MONSTER) {
        return -1;
    }
    if ((unit->initial_team_flags & BATTLE_TEAM_MASK) != (team & 0xff)) {
        return -1;
    }
    if (slot == UNIT_CAREER_JOB_INDEX_BARD && (flags & UNIT_FLAG_FEMALE)) {
        return -1;
    }
    if (slot == UNIT_CAREER_JOB_INDEX_DANCER && (flags & UNIT_FLAG_MALE)) {
        return -1;
    }
    total = unit->career.job_points.current.values[slot] + jp;
    if (total > UNIT_JOB_POINT_MAX) {
        total = UNIT_JOB_POINT_MAX;
    }
    unit->career.job_points.current.values[slot] = total;
    total = unit->career.job_points.total.values[slot] + jp;
    if (total > UNIT_JOB_POINT_MAX) {
        total = UNIT_JOB_POINT_MAX;
    }
    unit->career.job_points.total.values[slot] = total;
    level = main_job_calculate_level(total);
    levels = unit->career.job_levels[slot / 2];
    level_byte = level;
    if (slot & 1) {
        levels = level + (levels & 0xf0);
    } else {
        levels = (levels & 0xf) | (level_byte << 4);
    }
    unit->career.job_levels[slot / 2] = levels;
    main_job_store_unlock_bitset(
        unit->career.unlocked_jobs, main_job_calculate_unlocked(unit->career.job_levels, unit->unit_flags));
    return 0;
}

/* Find a free tile near a unit for it to be moved to, returning 0 or -1.
 *
 * Panels holding another unit or an earlier destination are marked in
 * unit_record_index. Rings of increasing Manhattan radius around the unit are then
 * searched, first up to radius 3 within -10..+4 of the unit's height, then up
 * to radius 16 at any height, for a flat enough, unblocked tile that is not
 * lava or an obstacle. The chosen tile is also appended to the destination
 * list.
 *
 * The panel clear walks the byte offset itself: the target loop has no index
 * register, which an index loop keeps. Only level's destination store is an
 * assignment chain; that shape alone gives the target's register choices. */
s32 battle_unit_find_relocation_tile(s32 unit_index, map_tile_coordinates_t* destination) {
    unit_stats_t* unit;
    map_tile_t* tile;
    /* Byte access retains the original packed load and field-extraction order. */
    s32 origin_x;
    s32 origin_y;
    s32 low;
    s32 high;
    s32 i;
    s32 n;
    s32 height;
    s32 pass;
    s32 limit;
    s32 dx;
    s32 dy;
    s32 x;
    s32 y;
    s32 side;
    s32 level;
    s32 index;
    s32 surface;
    u32 depth;
    map_tile_flags_t flags;
    s32 found_x;
    s32 found_y;
    s32 total;
    target_panel_t* panel;

    unit = &g_battle_unit_stats[unit_index];
    origin_x = unit->x;
    origin_y = unit->position.y;
    for (n = 0x1ff * sizeof(target_panel_t); n >= 0; n -= sizeof(target_panel_t)) {
        ((target_panel_t*)((u8*)g_target_panels + n))->unit_record_index = 0;
    }
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        unit = &g_battle_unit_stats[i];
        if (unit->entd_slot != BATTLE_ENTD_SLOT_NONE) {
            index = battle_map_calculate_location(unit);
            panel = &g_target_panels[index];
            tile = &g_map_tile_data[index];
            if (i == unit_index) {
                height = tile->height * 2 + tile->depth_half_height.half_height;
                low = height - 10;
                if (low < 0) {
                    low = 0;
                }
                high = height + 4;
            } else {
                panel->unit_record_index = 1;
            }
        }
    }
    for (i = 0; i < g_battle_relocated_unit_count; i++) {
        index = g_battle_relocated_unit_coords[i].elevation * 256 + g_battle_relocated_unit_coords[i].y * g_map_max_x
            + g_battle_relocated_unit_coords[i].x;
        panel = &g_target_panels[index];
        panel->unit_record_index = 1;
    }
    for (pass = 0; pass < 2; pass++) {
        limit = 4;
        if (pass != 0) {
            limit = 0x11;
        }
        for (i = 1; i < limit; i++) {
            for (dx = -i; dx <= i; dx++) {
                x = origin_x + dx;
                if (x < 0 || x >= g_map_max_x) {
                    continue;
                }
                if (dx > 0) {
                    dy = i - dx;
                } else {
                    dy = i + dx;
                }
                side = -1;
                found_x = x;
                for (y = origin_y - dy; side < 2; side += 2) {
                    if (y >= 0 && y < g_map_max_y) {
                        found_y = y;
                        for (level = 0; level < 2; level++) {
                            index = level * 256 + g_map_max_x * y + x;
                            tile = &g_map_tile_data[index];
                            depth = (*(u8*)&tile->depth_half_height);
                            if (tile->depth_half_height.depth >= 3) {
                                continue;
                            }
                            height = depth & MAP_TILE_HALF_HEIGHT_MASK;
                            if (height >= 3) {
                                continue;
                            }
                            flags = tile->flags;
                            if (flags.blocked) {
                                continue;
                            }
                            if (flags.untargetable) {
                                continue;
                            }
                            if (g_target_panels[index].unit_record_index != 0) {
                                continue;
                            }
                            surface = tile->surface.type;
                            if (surface == MAP_SURFACE_LAVA || surface == MAP_SURFACE_OBSTACLE) {
                                continue;
                            }
                            if (pass == 0) {
                                total = tile->height * 2 + height;
                                if (total > high || total < low) {
                                    continue;
                                }
                            }
                            destination->x = found_x;
                            g_battle_relocated_unit_coords[g_battle_relocated_unit_count].x = found_x;
                            g_battle_relocated_unit_coords[g_battle_relocated_unit_count].y = destination->y = found_y;
                            g_battle_relocated_unit_coords[g_battle_relocated_unit_count].elevation = destination->layer
                                = level;
                            g_battle_relocated_unit_count++;
                            return 0;
                        }
                    }
                    y += dy * 2;
                }
            }
        }
    }
    return -1;
}

move_effective_flags_e battle_move_get_effective_flags(const unit_stats_t* unit) {
    u8 flags;
    s32 movement_abilities_2;
    s32 movement_abilities_3;
    s32 current_status_3;

    current_status_3 = unit->status_sets.current[2];
    movement_abilities_3 = unit->ability_flags.movement[2];
    movement_abilities_2 = unit->ability_flags.movement[1];

    flags = 0;
    if (current_status_3 & STATUS_BYTE_MASK(STATUS_ID_FLOAT)) {
        movement_abilities_3 |= BATTLE_MOVEMENT_SET_3_FLOAT;
    }
    if (current_status_3 & (STATUS_BYTE_MASK(STATUS_ID_CHICKEN) | STATUS_BYTE_MASK(STATUS_ID_FROG))) {
        movement_abilities_3 &= (u8)~BATTLE_MOVEMENT_SET_3_FLY;
    }
    if (movement_abilities_3 & BATTLE_MOVEMENT_SET_3_MOVE_IN_WATER) {
        flags = BATTLE_EFFECTIVE_MOVEMENT_ON_WATER;
    } else if (movement_abilities_3 & BATTLE_MOVEMENT_SET_3_WALK_ON_WATER) {
        flags = BATTLE_EFFECTIVE_MOVEMENT_IN_WATER;
    }
    if (movement_abilities_3 & BATTLE_MOVEMENT_SET_3_SILENT_WALK) {
        flags |= BATTLE_EFFECTIVE_MOVEMENT_SILENT_WALK;
    }
    if (movement_abilities_3 & BATTLE_MOVEMENT_SET_3_FLY) {
        flags |= BATTLE_EFFECTIVE_MOVEMENT_FLY;
        if (movement_abilities_3 & BATTLE_MOVEMENT_SET_3_FLOAT) {
            /* The target also preserves undefined effective-movement bit 0x04. */
            flags = (flags
                        & (BATTLE_EFFECTIVE_MOVEMENT_FLY | BATTLE_EFFECTIVE_MOVEMENT_TELEPORT | 0x04
                            | BATTLE_EFFECTIVE_MOVEMENT_SILENT_WALK))
                | BATTLE_EFFECTIVE_MOVEMENT_FLOAT;
        }
    } else if (movement_abilities_2 & (BATTLE_MOVEMENT_SET_2_TELEPORT | BATTLE_MOVEMENT_SET_2_TELEPORT_2)) {
        flags |= BATTLE_EFFECTIVE_MOVEMENT_TELEPORT;
        if (movement_abilities_3 & BATTLE_MOVEMENT_SET_3_FLOAT) {
            flags = (flags
                        & (BATTLE_EFFECTIVE_MOVEMENT_FLY | BATTLE_EFFECTIVE_MOVEMENT_TELEPORT | 0x04
                            | BATTLE_EFFECTIVE_MOVEMENT_SILENT_WALK))
                | BATTLE_EFFECTIVE_MOVEMENT_FLOAT;
        }
    } else if (movement_abilities_3 & BATTLE_MOVEMENT_SET_3_FLOAT) {
        /* The target preserves Silent Walk and undefined bit 0x04 here. */
        flags = (flags & 0x06) | BATTLE_EFFECTIVE_MOVEMENT_FLOAT;
    } else if (movement_abilities_3 & BATTLE_MOVEMENT_SET_3_MOVE_IN_WATER) {
        flags |= BATTLE_EFFECTIVE_MOVEMENT_ON_WATER;
    } else if (movement_abilities_3 & BATTLE_MOVEMENT_SET_3_WALK_ON_WATER) {
        flags |= BATTLE_EFFECTIVE_MOVEMENT_IN_WATER;
    }
    return flags;
}

/* Amount Move-Get EXP or Move-Get JP gives: the number of steps walked
 * (g_move_path.count), rnd(1..Move) for a teleport and 1 for a failed one,
 * at most 99. */
u8 battle_unit_calculate_move_exp_jp_up_to_earn(const unit_stats_t* unit) {
    u8 amount = g_move_path.count;

    if (amount == 0xFE) {
        amount = (rand() * unit->move) / 0x8000 + 1;
    }
    if (amount == 0xFF) {
        amount = 1;
    }
    if (amount >= 0x64) {
        amount = 0x63;
    }
    return amount;
}

/* Run the ability formula for a preview of attacker's command against target.
 *
 * The attacker's 20 command bytes are saved, replaced by command, and
 * restored afterwards. Without the Auto flag in the loaded ability data, the
 * target's tile must be targetable; otherwise its action data is cleared. */
s32 battle_action_calculate_projected_effect(unit_stats_t* attacker, unit_stats_t* target, action_command_t* command) {
    u8 flags_3;
    s32 result;
    s32 saved_action_state;
    s32 saved_action_context;

    main_util_copy_action_data(&attacker->action_command, &g_action_saved_command);
    main_util_copy_action_data(command, &attacker->action_command);
    battle_action_init_current_ability_strike_data(attacker);
    saved_action_state = g_action_state;
    g_action_state = ACTION_STATE_PREVIEW;
    if (!(g_battle_loaded_ability_flags_1 & ABILITY_SECONDARY_FLAG_1_AUTO)) {
        result = battle_target_calculate_map_for_action(command, &flags_3);
        if (result != -1 && !g_map_tile_data[battle_map_calculate_location(target)].ceiling_depth_and_marks.targeted) {
            result = -1;
        }
        if (result == -1) {
            battle_action_clear_current_data(&target->action);
            target->action.hit = 0;
            target->action.attack_accuracy = 0;
            g_action_state = saved_action_state;
            main_util_copy_action_data(&g_action_saved_command, &attacker->action_command);
            return 0;
        }
    }
    saved_action_context = g_action_context;
    g_action_current_ability.defaulted_to_attack = 0;
    g_action_context = ACTION_CONTEXT_PRIMARY;
    target->action.reaction_id = 0;
    battle_action_run_pre_formula_setup(&attacker->action_command, target->battle_id);
    g_action_context = saved_action_context;
    g_action_state = saved_action_state;
    main_util_copy_action_data(&g_action_saved_command, &attacker->action_command);
    return 0;
}

/* The Elemental ability that the surface type of the unit's tile selects. */
s32 battle_action_get_elemental_ability_id(const unit_stats_t* unit) {
    return g_main_terrain_geomancy_abilities[g_map_tile_data[battle_map_calculate_location(unit)].surface.type];
}

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
    tile = &g_map_tile_data[battle_map_calculate_location(unit)];
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

u32 battle_move_set_target_for_mounted_unit_and_find_item(unit_stats_t* unit) {
    unit_stats_t* target;
    u32 flags;
    s32 i;
    s32 find_item;
    move_find_result_data_t* tile;

    /* Synthesize CRYSTAL_OR_TREASURE when the filtered tile search returns a
     * non-negative unit ID. The explicit sign test keeps the target nor/srl. */
    flags = (~battle_unit_find_at_tile(unit->x, unit->position.y, unit->position.higher_elevation,
                 BATTLE_UNIT_TILE_FILTER_CRYSTAL | BATTLE_UNIT_TILE_FILTER_TREASURE)
                >> 31)
        & 1;
    if (!(unit->mount_info & BATTLE_MOUNT_INFO_FLAG_RIDER)) {
        target = unit;
    } else {
        target = &g_battle_unit_stats[unit->mount_info & BATTLE_MOUNT_INFO_PARTNER_ID_MASK];
        g_action_target = target;
        g_action_target_data = &target->action;
        battle_action_clear_current_data(&target->action);
        for (i = 0; i < STATUS_BYTE_COUNT; i++) {
            g_action_target_data->status_changes.removal[i]
                = g_main_status_check_sets[MAIN_STATUS_CHECK_SET_MOUNT_REMOVAL][i];
        }
        if (battle_status_modify_inflictions(0) != 0) {
            g_action_target_data->attack_type = ACTION_TYPE_STATUS_CHANGE;
            battle_status_resolve_unit_changes(target->battle_id, 0);
            flags |= MOVE_POST_EVENT_MOUNT_STATUS_CHANGE;
        }
    }
    find_item = target->ability_flags.movement[2] & BATTLE_MOVEMENT_SET_3_MOVE_FIND_ITEM;
    tile = battle_map_set_item_trap_data(unit->x, unit->position.y, unit->position.higher_elevation);
    if (find_item && (tile->flags & MOVE_FIND_RESULT_ITEM)) {
        flags |= MOVE_POST_EVENT_ITEM_FOUND;
    } else if (tile->flags & MOVE_FIND_RESULT_TRAP) {
        flags |= MOVE_POST_EVENT_TRAP_TRIGGERED;
    }
    if (battle_status_remove_charging_ability_ct(unit, 0) != 0) {
        flags |= MOVE_POST_EVENT_CHARGING_CANCEL;
    }
    flags |= battle_move_get_support_flags(unit);
    return flags;
}

/* Post-move benefits of the unit's movement abilities: Move-HP Up, Move-MP Up,
 * Move-Get EXP and Move-Get JP, each with the common benefit bit. */
s32 battle_move_get_support_flags(const unit_stats_t* unit) {
    u8 flags = unit->ability_flags.movement[1];
    s32 post_event_flags = (0 - (unit->ability_flags.movement[0] & BATTLE_MOVEMENT_SET_1_MOVE_HP_UP))
        & (MOVE_POST_EVENT_MOVEMENT_BENEFIT | MOVE_POST_EVENT_MOVE_HP_UP);
    if (flags & BATTLE_MOVEMENT_SET_2_MOVE_MP_UP)
        post_event_flags |= MOVE_POST_EVENT_MOVEMENT_BENEFIT | MOVE_POST_EVENT_MOVE_MP_UP;
    if (flags & BATTLE_MOVEMENT_SET_2_MOVE_GET_EXP)
        post_event_flags |= MOVE_POST_EVENT_MOVEMENT_BENEFIT | MOVE_POST_EVENT_MOVE_GET_EXP;
    if (flags & BATTLE_MOVEMENT_SET_2_MOVE_GET_JP)
        post_event_flags |= MOVE_POST_EVENT_MOVEMENT_BENEFIT | MOVE_POST_EVENT_MOVE_GET_JP;
    return post_event_flags;
}
