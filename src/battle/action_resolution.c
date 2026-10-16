/* Action resolution, including knockback, reaction checks, pre-formula setup, attack finalization and status changes.
 */

#include "fft/battle_action.h"
#include "fft/battle_ai.h"
#include "fft/battle_formula.h"
#include "fft/battle_map.h"
#include "fft/battle_move.h"
#include "fft/battle_status.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "fft/data.h"
#include "fft/main.h"
#include "fft/main_item.h"
#include "fft/main_unit.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/unit.h"
#include "psx/types.h"

static s32 battle_action_add_poached_item_to_fur_shop_inventory(void);
static void battle_action_apply_jp_change(unit_stats_t* unit);
static void battle_action_check_arrow_guard_usability(void);
static void battle_action_check_blade_grasp_usability(void);
static void battle_action_check_counter_reaction_usability(u16 reaction_id, u32 mask);
static void battle_action_check_critical_hp_reaction_usability(s16 reaction_id);
static void battle_action_check_damage_split_usability(void);
static void battle_action_check_distribute_usability(void);
static void battle_action_check_mp_cost_reaction_usability(s16 reaction_id);
static void battle_action_check_damage_sharing_reaction_usability(void);
static void battle_action_check_mp_switch_usability(void);
static void battle_action_check_hp_damage_reaction_usability(s16 reaction_id);
static s32 battle_action_check_reaction(const unit_stats_t* unit);
static void battle_action_check_evasion_reactions(void);
static void battle_action_check_reflect_reaction(void);
static void battle_action_check_stat_save_and_restore_reaction_usability(void);
static void battle_action_clear_status_changes(action_data_t* action);
static void battle_action_dispatch_target_reaction_ability(void);
static s32 battle_action_remove_broken_or_stolen_equipment(void);
static void battle_action_set_target_variables(unit_stats_t* unit);
static s32 battle_status_is_unit_out_of_action(const unit_stats_t* unit);
static void battle_status_store_ability_cancellations(void);
static void battle_status_remove_control(unit_stats_t* unit);
static void battle_status_apply_action_removals(s32 unit_idx);
static void battle_status_store_for_current_attack(s32 unit_id, s32 removal_only);
static s32 battle_unit_apply_level_up_down_ability(void);
static s32 battle_unit_apply_stat_increment_decrement(s32 mod, u8* stat, u8 max, u8 min);
static void battle_unit_update_attacker_earned_experience(s32 outcome);

void battle_status_remove_transparent_if_jump_used(unit_stats_t* unit) {
    action_data_t* action = &g_battle_action_attacker_action_data;
    g_battle_action_attacker_data = action;
    /* Inflicted status byte 2 bit 0x10 is Transparent. */
    if (unit->action_command.skillset_id == SKILLSET_ID_JUMP
        && (unit->inflicted_status[STATUS_BYTE_INDEX(STATUS_ID_TRANSPARENT)]
            & STATUS_BYTE_MASK(STATUS_ID_TRANSPARENT))) {
        action->hit = 1;
        g_battle_action_attacker_action_data.attack_type |= ACTION_TYPE_STATUS_CHANGE;
        g_battle_action_attacker_action_data.status_changes.removal[2]
            = g_battle_action_attacker_action_data.status_changes.removal[2] | STATUS_BYTE_MASK(STATUS_ID_TRANSPARENT);
    }
}

/* Apply the ability's status. Returns 1 when it changes something or the
 * target is undead; otherwise the action is canceled as a forced failure and
 * 0 returned. */
s32 battle_formula_apply_status_and_check_undead(void) {
    battle_formula_apply_status();
    if (g_battle_action_target_data->attack_type & ACTION_TYPE_STATUS_CHANGE) {
        return 1;
    }
    if (g_battle_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_UNDEAD)]
        & STATUS_BYTE_MASK(STATUS_ID_UNDEAD)) {
        return 1;
    }
    battle_formula_nullify_action();
    g_battle_action_target_data->miss_type = ACTION_MISS_TYPE_FORCED_FAILURE;
    return 0;
}

/* Re-run the pre-formula setup against the current target as a plain weapon
 * Attack, then restore the attacker's skillset and ability and the strike
 * counter. It ends with the current ability's weapon_proc_pending and reaction_id
 * cleared. */
void battle_action_switch_ability_to_default_attack(void) {
    unit_stats_t* unit = g_battle_action_attacker;
    unit_stats_t* u2;
    unit_stats_t* u3;
    u8 saved_strike_counter = g_battle_action_current_ability.strike_counter;
    u8 saved_skillset = unit->action_command.skillset_id;
    u16 saved_ability;

    unit->action_command.skillset_id = SKILLSET_ID_ATTACK;
    u2 = g_battle_action_attacker;
    saved_ability = unit->action_command.ability_id;
    u2->action_command.ability_id = 0;
    battle_action_run_pre_formula_setup(&u2->action_command, g_battle_action_current_ability.target_id);
    u3 = g_battle_action_attacker;
    g_battle_action_current_ability.strike_counter = saved_strike_counter;
    u3->action_command.ability_id = saved_ability;
    u3->action_command.skillset_id = saved_skillset;
    g_battle_action_current_ability.weapon_proc_pending = 0;
    g_battle_action_current_ability.reaction_id = 0;
}

/* Reset the result records of the current target and the current attacker; the
 * attacker's starts as not hit. */
static void battle_action_clear_data(void) {
    battle_action_clear_current_data(g_battle_action_target_data);
    battle_action_clear_current_data(g_battle_action_attacker_data);
    g_battle_action_attacker_data->hit = 0;
}

/* Resolve a pending knockback as the target's own action. A grounded target
 * that is not a crystal, dead or petrified takes (fall height / 2 - Jump) *
 * MaxHP / 10 fall damage when that is positive, at most 999; a flying one
 * takes none. */
static void battle_move_apply_knockback(void) {
    u8* flags;
    unit_stats_t* unit;
    action_data_t* action;
    s32 amount;
    s32 diff;

    battle_action_clear_data();
    flags = &g_battle_action_current_ability.knockback_flags;
    *flags &= BATTLE_KNOCKBACK_KIND_MASK;
    g_battle_action_target_data->attack_type = ACTION_TYPE_PSEUDO_STATUS;
    unit = g_battle_action_target;
    amount = 0;
    if (!(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)] & STATUS_BYTE_MASK(STATUS_ID_CRYSTAL))
        && !(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DEAD)] & STATUS_BYTE_MASK(STATUS_ID_DEAD))
        && !(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_PETRIFY)] & STATUS_BYTE_MASK(STATUS_ID_PETRIFY))) {
        if (*flags == BATTLE_KNOCKBACK_FALL_DAMAGE) {
            diff = (g_battle_action_current_ability.knockback_fall_height >> 1) - unit->jump;
            if (diff > 0) {
                amount = (diff * unit->max_hp) / 10;
                if (amount >= 0x3E8) {
                    amount = 0x3E7;
                }
            }
        }
        action = g_battle_action_target_data;
        action->hp_damage = amount;
        if (amount != 0) {
            action->attack_type |= ACTION_TYPE_HP_DAMAGE;
        }
    }
}

/* Applies the pending reaction ability's effect to the current target action
 * record and finishes the strike. Returns 0 when the ability's behavior
 * flags select the alternate handler, -1 for an ability with no effect entry,
 * and 1 once the action has been finalized.
 *
 * The Damage Split case chains the damage store so the value reaches `healing` through
 * the target record; storing it there first is what orders the three global
 * pointer loads the way the target emits them. */
static s32 battle_action_perform_reaction_ability(void) {
    s16 ability_id;
    u8 attack_type = ACTION_TYPE_PSEUDO_STATUS;
    s32 healing;
    s32 count;

    ability_id = g_battle_current_reaction_ability_id;
    if (ability_id == ABILITY_ID_REACTION_REFLECT) {
        g_battle_action_attacker = &g_battle_unit_stats[g_battle_acting_unit_id];
    }
    if ((g_main_reaction_behavior_flags[ability_id - ABILITY_ID_REACTION_FIRST]
            & (BATTLE_REACTION_BEHAVIOR_FLAG_TRIGGER_ABILITY | BATTLE_REACTION_BEHAVIOR_FLAG_ABILITY))
        == 1) {
        return 0;
    }
    battle_action_clear_data();
    switch (g_battle_current_reaction_ability_id) {
    case ABILITY_ID_REACTION_PA_SAVE:
        g_battle_action_target_data->pa_change = 0x81;
        break;
    case ABILITY_ID_REACTION_MA_SAVE:
        g_battle_action_target_data->ma_change = 0x81;
        break;
    case ABILITY_ID_REACTION_SPEED_SAVE:
        g_battle_action_target_data->sp_change = 0x81;
        break;
    case ABILITY_ID_REACTION_SUNKEN_STATE:
        g_battle_action_target_data->status_changes.infliction[STATUS_BYTE_INDEX(STATUS_ID_TRANSPARENT)]
            = STATUS_BYTE_MASK(STATUS_ID_TRANSPARENT);
        if (battle_status_modify_inflictions(0) != 0) {
            attack_type = ACTION_TYPE_STATUS_CHANGE;
        }
        break;
    case ABILITY_ID_REACTION_CAUTION:
        g_battle_action_target_data->status_changes.infliction[STATUS_BYTE_INDEX(STATUS_ID_DEFENDING)]
            = STATUS_BYTE_MASK(STATUS_ID_DEFENDING);
        if (battle_status_modify_inflictions(0) != 0) {
            attack_type = ACTION_TYPE_STATUS_CHANGE;
        }
        break;
    case ABILITY_ID_REACTION_DRAGON_SPIRIT:
        g_battle_action_target_data->status_changes.infliction[STATUS_BYTE_INDEX(STATUS_ID_RERAISE)]
            = STATUS_BYTE_MASK(STATUS_ID_RERAISE);
        if (battle_status_modify_inflictions(0) != 0) {
            attack_type = ACTION_TYPE_STATUS_CHANGE;
        }
        break;
    case ABILITY_ID_REACTION_REGENERATOR:
        g_battle_action_target_data->status_changes.infliction[STATUS_BYTE_INDEX(STATUS_ID_REGEN)]
            = STATUS_BYTE_MASK(STATUS_ID_REGEN);
        if (battle_status_modify_inflictions(0) != 0) {
            attack_type = ACTION_TYPE_STATUS_CHANGE;
        }
        break;
    case ABILITY_ID_REACTION_BRAVE_UP:
        g_battle_action_target_data->brave_change = 0x83;
        break;
    case ABILITY_ID_REACTION_FAITH_UP:
        g_battle_action_target_data->faith_change = 0x83;
        break;
    case ABILITY_ID_REACTION_HP_RESTORE:
        attack_type = ACTION_TYPE_HP_HEALING;
        g_battle_action_target_data->hp_healing = g_battle_action_target->max_hp - g_battle_action_target->hp;
        break;
    case ABILITY_ID_REACTION_MP_RESTORE:
        attack_type = ACTION_TYPE_MP_HEALING;
        g_battle_action_target_data->mp_healing = g_battle_action_target->max_mp - g_battle_action_target->mp;
        break;
    case ABILITY_ID_REACTION_CRITICAL_QUICK:
        g_battle_action_target_data->ct_change = ACTION_CT_CHANGE_QUICK;
        break;
    case ABILITY_ID_REACTION_MEATBONE_SLASH:
        attack_type = ACTION_TYPE_HP_DAMAGE;
        g_battle_action_target_data->hp_damage = g_battle_action_attacker->max_hp;
        break;
    case ABILITY_ID_REACTION_ABSORB_USED_MP:
        attack_type = ACTION_TYPE_MP_HEALING;
        g_battle_action_target_data->mp_healing = g_battle_action_target_data->last_received_attack;
        break;
    case ABILITY_ID_REACTION_GILGAME_HEART:
        g_battle_action_target_data->gil_change = g_battle_action_target_data->last_received_attack;
        break;
    case ABILITY_ID_REACTION_DISTRIBUTE:
        count = g_battle_distribute_target_count;
        if (count != 0) {
            healing = (g_battle_action_attacker->action.last_received_attack + count - 1) / count;
        } else {
            healing = 0;
        }
        attack_type = ACTION_TYPE_HP_HEALING;
        g_battle_action_target_data->hp_healing = healing;
        break;
    case ABILITY_ID_REACTION_DAMAGE_SPLIT:
        healing = g_battle_action_target_data->hp_damage = g_battle_action_attacker->action.last_received_attack;
        attack_type = ACTION_TYPE_HP_DAMAGE;
        g_battle_action_attacker_data->hp_healing = healing;
        g_battle_action_attacker_data->attack_type = ACTION_TYPE_HP_HEALING;
        g_battle_action_attacker_data->hit = 1;
        break;
    default:
        g_battle_action_target_data->attack_type = 0;
        return -1;
    }
    g_battle_action_target_data->attack_type = attack_type;
    battle_action_finalize_target_current_action();
    if (g_battle_action_target_data->special_effect != 0) {
        g_battle_action_target_data->attack_type |= ACTION_TYPE_PSEUDO_STATUS;
    }
    g_battle_action_current_ability.reaction_id = 0;
    return 1;
}

/* Record the weapon the current strike uses: the primary hand on the first strike and the
 * secondary after it; none for an ordinary ability that neither uses weapon range nor strikes with
 * the weapon, and none for Jump; the chosen item for the item and katana menus. An empty hand
 * (0xFF) is stored as none. */
void battle_action_store_used_weapon(action_command_t* action) {
    u8 weapon;
    s16 ability_id;

    if (g_battle_action_current_ability.strike_counter == 0) {
        weapon = g_battle_action_current_ability.primary_weapon_id;
    } else {
        weapon = g_battle_action_current_ability.secondary_weapon_id;
    }
    switch (g_main_action_menu_types_by_skillset[action->skillset_id]) {
    case ACTION_MENU_TYPE_DEFAULT:
    case ACTION_MENU_TYPE_ARITHMETICKS:
    case ACTION_MENU_TYPE_ELEMENTS:
    case ACTION_MENU_TYPE_MONSTER:
        ability_id = action->ability_id;
        if (ability_id < ABILITY_ID_ITEM_FIRST) {
            if ((g_main_ability_range_data[ability_id].flags_1
                    & (ABILITY_SECONDARY_FLAG_1_WEAPON_RANGE | ABILITY_SECONDARY_FLAG_1_WEAPON_STRIKE))
                == 0) {
                weapon = ITEM_ID_NOTHING;
            }
        }
        break;
    case ACTION_MENU_TYPE_ITEM_INVENTORY:
    case ACTION_MENU_TYPE_KATANA_INVENTORY:
        /* Retail reads only the low byte of the 16-bit item/equipment field. */
        weapon = action->item_or_equipment;
        break;
    case ACTION_MENU_TYPE_JUMP:
    case ACTION_MENU_TYPE_UNKNOWN_0F:
        weapon = ITEM_ID_NOTHING;
        break;
    }
    if (weapon == ITEM_ID_NONE) {
        weapon = ITEM_ID_NOTHING;
    }
    g_battle_action_current_ability.weapon_id = weapon;
}

/* g_battle_action_current_ability bound a second time. Spelling every field through one
 * symbol lets GCC share a base register between the two access groups below,
 * which changes this function's code. */
extern battle_current_ability_t g_battle_action_current_ability_view;

/* Pre-formula setup: loads the current ability, weapon and item data for one
 * attacker/target pair and runs the formula handler.
 *
 * Returns 1 when the target was already missed before the formula (reflect,
 * Blade Grasp, Arrow Guard) and 0 otherwise, including the reaction and
 * knockback early exits. id holds a weapon or item id, then a status-infliction
 * index. */
s32 battle_action_run_pre_formula_setup(const action_command_t* source, u8 target_id) {
    action_command_t action;
    weapon_data_t* weapon;
    ability_secondary_data_t* ability;
    item_secondary_data_t* item;
    ability_data_t* data;
    u8 menu_type;
    u8 skillset;
    u8 formula;
    u8 id;
    u16 ability_id;

    main_util_copy_action_data(source, &action);
    g_battle_action_current_ability.target_id = target_id;
    g_battle_action_target = &g_battle_unit_stats[target_id];
    g_battle_action_target_data = &g_battle_unit_stats[target_id].action;
    g_battle_action_current_ability.post_action_message = BATTLE_POST_ACTION_MESSAGE_NONE;
    g_battle_action_current_ability_view.attacker_id = action.actor_id;
    g_battle_action_attacker_data = &g_battle_action_attacker_action_data;
    g_battle_action_attacker = &g_battle_unit_stats[g_battle_action_current_ability_view.attacker_id];
    if (g_battle_action_context != ACTION_CONTEXT_PRIMARY && battle_action_perform_reaction_ability() != 0) {
        battle_action_store_used_weapon(&action);
        return 0;
    }
    if (g_battle_action_current_ability.knockback_flags & BATTLE_KNOCKBACK_PENDING) {
        battle_move_apply_knockback();
        return 0;
    }
    g_battle_action_current_ability_view.terrain
        = g_battle_map_tile_data[battle_map_calculate_location(g_battle_action_target)].surface.type;
    if (g_battle_action_current_ability.strike_counter == 0) {
        id = g_battle_action_current_ability.primary_weapon_id;
    } else {
        id = g_battle_action_current_ability.secondary_weapon_id;
    }
    if (action.skillset_id == SKILLSET_ID_DRAW_OUT) {
        /* Item selection reads only the low byte of the 16-bit item/equipment field. */
        id = action.item_or_equipment;
    }
    if (id >= 0x80) {
        id = ITEM_ID_NOTHING;
    }
    g_battle_action_current_ability_view.weapon_id = id;
    g_battle_action_current_ability_view.ability_id = action.ability_id;
    ability_id = action.ability_id;
    g_battle_action_current_ability.two_hands_active = 0;
    if (g_battle_action_current_ability_view.ability_id > ABILITY_ID_ITEM_FIRST) {
        ability_id = 0;
    }
    weapon = &g_main_item_weapon_data[id];
    main_mem_copy_byte_data_source_first(&g_main_ability_range_data[ability_id],
        &g_battle_action_current_ability_view.range_data, sizeof(ability_secondary_data_t));
    main_mem_copy_byte_data_source_first(
        weapon, &g_battle_action_current_ability_view.weapon_data, sizeof(weapon_data_t));
    g_battle_action_current_ability.proc_id = weapon->proc_id;
    g_battle_action_current_ability.charge_power = 0;
    g_battle_action_current_ability.formula = ABILITY_FORMULA_WEAPON_DAMAGE;
    g_battle_action_current_ability.attacker_faith = g_battle_action_attacker->faith;
    g_battle_action_current_ability.target_faith = g_battle_action_target->faith;
    battle_action_clear_data();
    skillset = action.skillset_id;
    g_battle_action_current_ability.skillset = skillset;
    menu_type = g_main_action_menu_types_by_skillset[skillset];
    if (g_battle_action_current_ability_view.weapon_proc_pending != 0) {
        action.ability_id = g_battle_action_current_ability_view.reaction_id;
        g_battle_action_current_ability_view.ability_id = action.ability_id;
        menu_type = ACTION_MENU_TYPE_DEFAULT;
    } else {
        g_battle_action_current_ability_view.reaction_id = 0;
    }
    data = &g_main_ability_data[action.ability_id];
    g_battle_action_target_data->miss_display_flags = data->ai_flags.flags_1 & ACTION_MISS_DISPLAY_TEAM_MASK;
    if (data->ai_flags.flags_4 & AI_ABILITY_FLAG_4_EVADE_WITH_MOTION) {
        g_battle_action_target_data->miss_display_flags += ACTION_MISS_DISPLAY_DODGE_ANIMATION;
    }
    if (menu_type == ACTION_MENU_TYPE_KATANA_INVENTORY) {
        id = action.item_or_equipment;
        action.ability_id = id + (ABILITY_ID_DRAW_OUT_ASURA - ITEM_ID_ASURA_KNIFE);
    }
    switch (menu_type) {
    case ACTION_MENU_TYPE_ELEMENTS:
        action.ability_id = battle_action_get_elemental_ability_id(g_battle_action_attacker);
        /* fall through */
    case ACTION_MENU_TYPE_DEFAULT:
    case ACTION_MENU_TYPE_ARITHMETICKS:
    case ACTION_MENU_TYPE_MONSTER:
    case ACTION_MENU_TYPE_KATANA_INVENTORY:
        formula = ABILITY_FORMULA_WEAPON_DAMAGE;
        if (action.ability_id <= ABILITY_ID_ITEM_FIRST) {
            if ((g_battle_action_attacker->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FROG)]
                    & STATUS_BYTE_MASK(STATUS_ID_FROG))
                && g_battle_action_current_ability_view.weapon_proc_pending == 0) {
                g_battle_action_current_ability_view.weapon_id = ITEM_ID_NOTHING;
                main_mem_copy_byte_data_source_first(
                    g_main_item_weapon_data, &g_battle_action_current_ability_view.weapon_data, sizeof(weapon_data_t));
            }
            ability = &g_main_ability_range_data[action.ability_id];
            main_mem_copy_byte_data_source_first(
                ability, &g_battle_action_current_ability_view.range_data, sizeof(ability_secondary_data_t));
            g_battle_action_current_ability.proc_id = ability->status_infliction_id;
            formula = ability->formula;
            if (!(g_battle_action_current_ability.range_data.flags_1
                    & (ABILITY_SECONDARY_FLAG_1_WEAPON_RANGE | ABILITY_SECONDARY_FLAG_1_WEAPON_STRIKE))
                && menu_type != ACTION_MENU_TYPE_KATANA_INVENTORY) {
                g_battle_action_current_ability_view.weapon_id = ITEM_ID_NOTHING;
            }
        }
        break;
    case ACTION_MENU_TYPE_ITEM_INVENTORY:
        formula = ABILITY_FORMULA_WEAPON_DAMAGE;
        id = action.item_or_equipment;
        if (id >= ITEM_ID_CONSUMABLE_FIRST) {
            g_battle_action_current_ability_view.weapon_id = id;
            id += ITEM_ID_COUNT - ITEM_ID_CONSUMABLE_FIRST;
            item = &g_main_item_secondary_data[id];
            g_battle_action_current_ability.used_item_id = id;
            g_battle_action_current_ability.proc_id = item->status_infliction_id;
            main_mem_zero(&g_battle_action_current_ability_view.range_data, sizeof(ability_secondary_data_t));
            formula = item->formula;
        }
        break;
    case ACTION_MENU_TYPE_WEAPON_INVENTORY:
        formula = ABILITY_FORMULA_DAMAGE_SP_TIMES_WP;
        break;
    case ACTION_MENU_TYPE_JUMP:
        formula = ABILITY_FORMULA_DAMAGE_PA_TIMES_WP;
        break;
    case ACTION_MENU_TYPE_CHARGE:
        ability_id = action.ability_id;
        if (ability_id < ABILITY_ID_CHARGE_FIRST || ability_id >= ABILITY_ID_MATH_FIRST) {
            ability_id = ABILITY_ID_CHARGE_FIRST;
        }
        g_battle_action_current_ability.charge_power
            = (g_main_charge_ability_data - ABILITY_ID_CHARGE_FIRST)[ability_id].power;
        /* fall through */
    case ACTION_MENU_TYPE_ATTACK:
        formula = weapon->formula;
        if ((g_battle_action_current_ability.weapon_data.flags & WEAPON_FLAG_TWO_HANDS_COMPATIBLE)
            && (g_battle_action_attacker->ability_flags.support[2] & BATTLE_SUPPORT_SET_3_TWO_HANDS)) {
            if (g_battle_action_attacker->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] == ITEM_ID_NONE
                && g_battle_action_attacker->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD] == ITEM_ID_NONE) {
                g_battle_action_current_ability.two_hands_active = 1;
            }
            if (g_battle_action_attacker->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON] == ITEM_ID_NONE
                && g_battle_action_attacker->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD] == ITEM_ID_NONE) {
                g_battle_action_current_ability.two_hands_active = 1;
            }
        }
        break;
    default:
        formula = ABILITY_FORMULA_WEAPON_DAMAGE;
        break;
    }
    g_battle_action_current_ability_view.weapon_proc_pending = 0;
    g_battle_action_current_ability.knockback_flags = 0;
    if (formula == 0 || formula >= ABILITY_FORMULA_ID_LIMIT) {
        formula = ABILITY_FORMULA_WEAPON_DAMAGE;
    }
    id = g_battle_action_current_ability_view.proc_id;
    if (formula == ABILITY_FORMULA_GUN_DAMAGE) {
        id = 0;
    }
    main_mem_copy_byte_data_source_first(&g_main_status_infliction_data[id],
        &g_battle_action_current_ability_view.status_infliction, sizeof(status_infliction_data_t));
    battle_status_store_ability_cancellations();
    g_battle_action_current_ability.formula = formula;
    battle_action_check_evasion_reactions();
    if (g_battle_action_target_data->hit == 0) {
        return 1;
    }
    g_battle_formula_handlers[formula - 1]();
    if (formula < 7) {
        battle_formula_apply_poach_and_train();
    }
    if (formula == ABILITY_FORMULA_DAMAGE_PA_TIMES_WP) {
        g_battle_action_current_ability_view.weapon_id = ITEM_ID_NOTHING;
    }
    battle_action_finalize_target_current_action();
    if (g_battle_action_target_data->special_effect != 0) {
        g_battle_action_target_data->attack_type |= ACTION_TYPE_PSEUDO_STATUS;
    }
    return 0;
}

/* For an ability whose status infliction type is cancel, copies its status bytes
 * to g_battle_action_canceled_statuses. Run during the pre-formula setup. */
static void battle_status_store_ability_cancellations(void) {
    s32 i;
    if (g_battle_action_current_ability.status_infliction.type & STATUS_INFLICTION_TYPE_CANCEL) {
        for (i = 0; i < STATUS_BYTE_COUNT; i++) {
            g_battle_action_canceled_statuses[i] = g_battle_action_current_ability.status_infliction.statuses[i];
        }
    }
}

/* Validate and clamp the target's current action result before it applies.
 *
 * A Dead target (unless the ability cancels Dead and the target is not
 * undead), or a petrified or walled one the ability does not cancel,
 * turns the action into a miss. Otherwise zero accuracy clears the hit flag,
 * HP/MP changes are capped at 999, a lethal HP hit drops knockback, and a
 * hit absorbed by the target team's Golem becomes a guarded miss. */
void battle_action_finalize_target_current_action(void) {
    action_data_t* action;
    u8 status = g_battle_action_target->status_sets.current[0];
    s32 invalid = 0;
    s32 team;

    if ((status & STATUS_BYTE_MASK(STATUS_ID_DEAD))
        && (!(g_battle_action_canceled_statuses[STATUS_BYTE_INDEX(STATUS_ID_DEAD)] & STATUS_BYTE_MASK(STATUS_ID_DEAD))
            || (status & STATUS_BYTE_MASK(STATUS_ID_UNDEAD)))) {
        invalid = 1;
    }
    if ((g_battle_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_PETRIFY)]
            & STATUS_BYTE_MASK(STATUS_ID_PETRIFY))
        && !(g_battle_action_canceled_statuses[STATUS_BYTE_INDEX(STATUS_ID_PETRIFY)]
            & STATUS_BYTE_MASK(STATUS_ID_PETRIFY))) {
        invalid = 1;
    }
    if ((g_battle_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_WALL)]
            & STATUS_BYTE_MASK(STATUS_ID_WALL))
        && !(g_battle_action_canceled_statuses[STATUS_BYTE_INDEX(STATUS_ID_WALL)] & STATUS_BYTE_MASK(STATUS_ID_WALL))) {
        invalid = 1;
    }
    if (invalid != 0) {
        battle_target_remove_ability_target(g_battle_action_target->battle_id);
        battle_action_clear_data();
        battle_formula_clear_pending_proc();
        battle_action_clear_knockback_flag();
        g_battle_action_current_ability.post_action_message = BATTLE_POST_ACTION_MESSAGE_NONE;
        battle_formula_force_attack_miss();
        g_battle_action_target_data->miss_type = ACTION_MISS_TYPE_CANCELED;
        return;
    }
    if (g_battle_action_target_data->attack_accuracy == 0) {
        g_battle_action_target_data->hit = 0;
    }
    if (g_battle_action_target_data->hp_damage >= 1000) {
        g_battle_action_target_data->hp_damage = 999;
    }
    if (g_battle_action_target_data->hp_healing >= 1000) {
        g_battle_action_target_data->hp_healing = 999;
    }
    if (g_battle_action_target_data->mp_damage >= 1000) {
        g_battle_action_target_data->mp_damage = 999;
    }
    if (g_battle_action_target_data->mp_healing >= 1000) {
        g_battle_action_target_data->mp_healing = 999;
    }
    if (g_battle_action_target_data->hp_damage >= g_battle_action_target->hp) {
        battle_action_clear_knockback_flag();
        battle_formula_clear_pending_proc();
    }
    team = (g_battle_action_target->initial_team_flags & BATTLE_TEAM_MASK) >> BATTLE_TEAM_SHIFT;
    action = g_battle_action_target_data;
    if ((action->attack_type != 0) && (action->hit != 0) && (g_battle_action_context == ACTION_CONTEXT_PRIMARY)
        && (g_battle_team_golem[team] != 0)
        && (g_battle_action_current_ability.range_data.flags_3 & ABILITY_SECONDARY_FLAG_3_BLOCKED_BY_GOLEM)) {
        action->miss_type = ACTION_MISS_TYPE_GOLEM_GUARD;
        action->special_effect = ACTION_SPECIAL_EFFECT_GOLEM_GUARD;
        battle_action_clear_status_changes(g_battle_action_target_data);
        battle_action_clear_knockback_flag();
    }
}

/* Drop a pending weapon proc together with the added-effect flag (special
 * effect 0x0200). */
void battle_formula_clear_pending_proc(void) {
    u16* pflag = &g_battle_action_current_ability.weapon_proc_pending;
    action_data_t* action;
    u16* special_effect;
    if (*pflag != 0) {
        action = g_battle_action_target_data;
        g_battle_action_current_ability.reaction_id = 0;
        *pflag = 0;
        /* Clear the added-effect bit (0x0200). Reading the field through a
         * second pointer keeps the lhu below the two scalar global stores;
         * a direct action->special_effect load is hoisted above them
         * (MEM_IN_STRUCT_P) and the function comes out 4 bytes short. */
        special_effect = &action->special_effect;
        *special_effect &= ~ACTION_SPECIAL_EFFECT_ADDED_EFFECT;
    }
}

/* Cancel a pending knockback: clear the current ability's knockback flags and the
 * target's KNOCKBACK special effect. */
void battle_action_clear_knockback_flag(void) {
    action_data_t* target_data;
    if ((g_battle_action_current_ability.knockback_flags & BATTLE_KNOCKBACK_PENDING) != 0) {
        target_data = g_battle_action_target_data;
        g_battle_action_current_ability.knockback_flags = 0;
        target_data->special_effect &= ~ACTION_SPECIAL_EFFECT_KNOCKBACK;
    }
}

/* Reset an action result record to "hit, 100% accuracy, no effect".
 *
 * Bytes 0x00-0x0d, 0x10-0x25 and 0x28-0x29 are cleared; reaction_id (0x0e) and
 * last_received_attack (0x26) keep their values. The canceled-status list is
 * cleared with the record's status changes. */
void battle_action_clear_current_data(action_data_t* action) {
    main_mem_zero(action, 0xE);
    main_mem_zero(&action->special_effect, 0x16);
    main_mem_zero(&action->exp_change, 2);
    /* The target stores the accuracy as a 16-bit value (0x2a..0x2b). */
    action->attack_accuracy = 100;
    action->attack_type = 0;
    action->hit = 1;
    battle_action_clear_status_changes(action);
}

/* Clear the record's status inflictions and removals, and the shared list of
 * canceled statuses with them. */
static void battle_action_clear_status_changes(action_data_t* action) {
    s32 i;
    for (i = 0; i < STATUS_BYTE_COUNT; i++) {
        g_battle_action_canceled_statuses[i] = 0;
        action->status_changes.infliction[i] = 0;
        action->status_changes.removal[i] = 0;
    }
}

#define STATUS_MASK(id) STATUS_BYTE_MASK(STATUS_ID_##id)

/* Apply the target's pending action results and flag reactions.
 *
 * Returns -1 for an invalid or absent unit, or when a mounted target ends
 * the action dead or crystallized; otherwise 0.
 *
 * The HP/MP damage and healing fields are read signed (lh), hence the s16
 * casts on the u16 header fields. The KO test's three status-bit checks fold
 * into the target's single 16-bit load of current[0..1] & 0x160. */
s32 battle_action_apply_target_result(s32 unit_id) {
    s32 outcome;
    s32 hp;
    s32 mp;
    s32 i;
    u16 old_hp;
    u16 old_mp;
    u8 old_status[5];
    u8 old_status_ct[16];

    outcome = 0;
    if (unit_id >= 21) {
        return -1;
    }
    g_battle_action_current_ability.target_id = unit_id;
    g_battle_action_target = &g_battle_unit_stats[unit_id];
    g_battle_action_target_data = &g_battle_unit_stats[unit_id].action;
    if (g_battle_action_target->entd_slot == BATTLE_ENTD_SLOT_NONE) {
        return -1;
    }
    if ((g_battle_action_target->status_sets.current[0] & STATUS_MASK(DEAD))
        && ((g_battle_action_target_data->status_changes.infliction[0] & STATUS_MASK(CRYSTAL))
            || (g_battle_action_target_data->status_changes.infliction[1] & STATUS_MASK(TREASURE)))) {
        battle_status_resolve_unit_changes(unit_id, 0);
        return 0;
    }
    if (g_battle_action_context == ACTION_CONTEXT_PRIMARY) {
        g_battle_action_target->ability_outcome = BATTLE_ABILITY_OUTCOME_TARGETED;
    }
    battle_action_dispatch_target_reaction_ability();
    if (g_battle_action_target_data->reaction_id == ABILITY_ID_REACTION_CATCH) {
        battle_action_increment_item_quantity_for_steal_break(
            g_battle_action_target, g_battle_action_target_data->last_received_attack);
    }
    if (g_battle_action_target_data->hit == 0) {
        return 0;
    }
    if (g_battle_action_target_data->special_effect & ACTION_SPECIAL_EFFECT_GOLEM_GUARD) {
        u16* golem;

        golem = &g_battle_team_golem[(g_battle_action_target->initial_team_flags & BATTLE_TEAM_MASK)
            >> BATTLE_TEAM_SHIFT];
        hp = *golem - g_battle_action_target_data->hp_damage;
        if (hp < 0) {
            hp = 0;
        }
        *golem = hp;
        return 0;
    }
    if (g_battle_action_target_data->special_effect & ACTION_SPECIAL_EFFECT_SET_GOLEM) {
        outcome = 1;
        g_battle_team_golem[(g_battle_action_target->team_flags & BATTLE_TEAM_MASK) >> BATTLE_TEAM_SHIFT]
            = g_battle_action_target->max_hp;
    }
    battle_action_check_damage_sharing_reaction_usability();
    old_hp = g_battle_action_target->hp;
    hp = old_hp - g_battle_action_target_data->hp_damage + g_battle_action_target_data->hp_healing;
    if (hp < 0) {
        hp = 0;
    }
    if (g_battle_action_target->max_hp < hp) {
        hp = g_battle_action_target->max_hp;
    }
    old_mp = g_battle_action_target->mp;
    mp = old_mp - g_battle_action_target_data->mp_damage + g_battle_action_target_data->mp_healing;
    if (mp < 0) {
        mp = 0;
    }
    if (g_battle_action_target->max_mp < mp) {
        mp = g_battle_action_target->max_mp;
    }
    if (old_hp != hp || old_mp != mp) {
        outcome = 1;
    }
    g_battle_action_target->hp = hp;
    g_battle_action_target->mp = mp;
    outcome |= battle_unit_apply_stat_increment_decrement(g_battle_action_target_data->sp_change,
        &g_battle_action_target->base_attributes[UNIT_ATTRIBUTE_SPEED], UNIT_SPEED_MAX, 1);
    outcome |= battle_unit_apply_stat_increment_decrement(
        g_battle_action_target_data->ct_change, &g_battle_action_target->ct, 255, 0);
    outcome |= battle_unit_apply_stat_increment_decrement(g_battle_action_target_data->pa_change,
        &g_battle_action_target->base_attributes[UNIT_ATTRIBUTE_PHYSICAL_ATTACK], UNIT_ATTACK_STAT_MAX, 1);
    outcome |= battle_unit_apply_stat_increment_decrement(g_battle_action_target_data->ma_change,
        &g_battle_action_target->base_attributes[UNIT_ATTRIBUTE_MAGIC_ATTACK], UNIT_ATTACK_STAT_MAX, 1);
    outcome |= battle_unit_apply_stat_increment_decrement(g_battle_action_target_data->brave_change,
        &g_battle_action_target->brave, UNIT_BRAVE_FAITH_MAX,
        (g_battle_action_target->mount_info & BATTLE_MOUNT_INFO_FLAG_RIDER) ? 10 : 0);
    outcome |= battle_unit_apply_stat_increment_decrement(
        g_battle_action_target_data->faith_change, &g_battle_action_target->faith, UNIT_BRAVE_FAITH_MAX, 0);
    outcome |= battle_action_remove_broken_or_stolen_equipment();
    if (g_battle_action_target_data->special_effect & ACTION_SPECIAL_EFFECT_DRAW_OUT_KATANA_NOT_BROKEN) {
        battle_action_increment_item_quantity_for_steal_break(
            g_battle_action_target, g_battle_action_target->action_command.item_or_equipment);
    }
    if (g_battle_action_target_data->gil_change != 0 || g_battle_action_target_data->exp_change != 0) {
        outcome |= 1;
    }
    battle_action_add_war_funds(g_battle_action_target, g_battle_action_target_data->gil_change, ITEM_ID_NOTHING);
    battle_action_apply_exp_change(g_battle_action_target, g_battle_action_target_data->exp_change);
    if (g_battle_action_target_data->jp_change != 0) {
        battle_action_apply_jp_change(g_battle_action_target);
    }
    outcome |= battle_unit_apply_level_up_down_ability();
    outcome |= battle_action_add_poached_item_to_fur_shop_inventory();
    if (g_battle_action_target_data->special_effect & ACTION_SPECIAL_EFFECT_MORBOL) {
        outcome |= 1;
        if (g_battle_action_state == ACTION_STATE_EXECUTE) {
            battle_status_clear_all(g_battle_action_target);
        }
    }
    if (hp == 0 && !(g_battle_action_target->status_sets.current[0] & STATUS_MASK(CRYSTAL))
        && !(g_battle_action_target->status_sets.current[0] & STATUS_MASK(DEAD))
        && !(g_battle_action_target->status_sets.current[1] & STATUS_MASK(TREASURE))) {
        battle_action_clear_status_changes(g_battle_action_target_data);
        g_battle_action_target_data->status_changes.infliction[0] = STATUS_MASK(DEAD);
        battle_status_resolve_unit_changes(unit_id, 1);
        if (g_battle_action_state == ACTION_STATE_EXECUTE) {
            g_battle_action_target->ko_count++;
        }
        battle_unit_update_attacker_earned_experience(2);
        return -(g_battle_action_target->mount_info >> 7);
    }
    if (hp <= (g_battle_action_target->max_hp / 5)) {
        g_battle_action_target_data->status_changes.infliction[2] |= STATUS_MASK(CRITICAL);
    } else {
        g_battle_action_target_data->status_changes.removal[2] |= STATUS_MASK(CRITICAL);
    }
    if (g_battle_action_target->brave < 10) {
        g_battle_action_target_data->status_changes.infliction[2] |= STATUS_MASK(CHICKEN);
    } else {
        g_battle_action_target_data->status_changes.removal[2] |= STATUS_MASK(CHICKEN);
    }
    if (g_battle_action_target_data->attack_type & ACTION_TYPE_HP_DAMAGE) {
        g_battle_action_target_data->status_changes.removal[4] |= STATUS_MASK(CHARM) | STATUS_MASK(SLEEP);
        g_battle_action_target_data->status_changes.removal[2] |= STATUS_MASK(TRANSPARENT);
        g_battle_action_target_data->status_changes.removal[1] |= STATUS_MASK(CONFUSION);
    }
    if ((g_battle_action_target_data->special_effect & ACTION_SPECIAL_EFFECT_KNOCKBACK)
        && battle_status_remove_charging_ability_ct(g_battle_action_target, 0) != 0) {
        g_battle_action_target_data->status_changes.removal[0] |= STATUS_MASK(CHARGING);
        g_battle_action_target->charged_ability_ct = 0xff;
    }
    battle_status_modify_inflictions(0);
    for (i = 0; i < 5; i++) {
        old_status[i] = g_battle_action_target->status_sets.current[i];
    }
    for (i = 0; i < 16; i++) {
        old_status_ct[i] = g_battle_action_target->status_ct[i];
    }
    battle_status_resolve_unit_changes(unit_id, 0);
    if ((g_battle_action_target->status_sets.current[0] & STATUS_MASK(DEAD)) && !(old_status[0] & STATUS_MASK(DEAD))) {
        outcome = 2;
        if (g_battle_action_state == ACTION_STATE_EXECUTE) {
            g_battle_action_target->ko_count++;
        }
    } else {
        for (i = 0; i < 5; i++) {
            if (g_battle_action_target->status_sets.current[i] != old_status[i]) {
                outcome = 1;
                break;
            }
        }
        for (i = 0; i < 16; i++) {
            if (g_battle_action_target->status_ct[i] != old_status_ct[i]) {
                outcome = 1;
                break;
            }
        }
    }
    battle_unit_update_attacker_earned_experience(outcome);
    if (outcome != 0) {
        g_battle_action_target->ability_outcome = BATTLE_ABILITY_OUTCOME_AFFECTED;
    }
    battle_action_check_stat_save_and_restore_reaction_usability();
    if (g_battle_action_target->status_sets.current[0] & (STATUS_MASK(CRYSTAL) | STATUS_MASK(DEAD))) {
        if (g_battle_action_target->mount_info & BATTLE_MOUNT_INFO_FLAG_RIDER) {
            return -1;
        }
    }
    return 0;
}

/* Add a poached monster's item to the Fur Shop inventory: the rare one when a
 * roll of 0..255 is below 31 (31 in 256), otherwise the common one.
 *
 * Inventory quantities saturate at 0xff. */
static s32 battle_action_add_poached_item_to_fur_shop_inventory(void) {
    unit_stats_t* unit;
    s32 idx;
    s32 item;
    s32 count;
    s32 result;

    if (g_battle_action_state != ACTION_STATE_EXECUTE) {
        return 0;
    }
    if ((g_battle_action_target_data->special_effect & ACTION_SPECIAL_EFFECT_POACH) == 0) {
        return 0;
    }
    unit = g_battle_action_target;
    if (unit->job_id < JOB_ID_CHOCOBO || unit->job_id > JOB_ID_TIAMAT) {
        return 0;
    }
    idx = unit->job_id - JOB_ID_CHOCOBO;
    if (main_util_roll_pass_fail(0x100, 0x1f) != 0) {
        item = g_main_item_poach_table[idx].common_item;
    } else {
        item = g_main_item_poach_table[idx].rare_item;
    }
    count = g_main_item_poached_quantities[item];
    result = 0xff;
    if (count != result) {
        result = count + 1;
        g_main_item_poached_quantities[item] = result;
    }
    return result;
}

/* Transform a unit into a Morbol, keeping its name, position and facing. */
void battle_unit_transform_into_morbol(unit_stats_t* unit) {
    u8 saved_name[0x10];
    u8 palette[8];
    u8* name;
    u8 unit_id;
    u8 formation_index;
    u8 x;
    u8 y;
    u8 higher_elevation;
    u8 facing;

    /* Preserve the unit's encoded name across the reset. */
    name = unit->name;
    unit_id = unit->unit_id;
    unit->job_id = JOB_ID_MORBOL;
    unit->character_identity = CHARACTER_IDENTITY_MONSTER;
    main_mem_copy_byte_data_source_first(name, saved_name, 0x10);
    formation_index = unit->formation_index;
    x = unit->x;
    y = unit->position.y;
    higher_elevation = unit->position.higher_elevation;
    facing = unit->position.facing;
    unit->formation_index = BATTLE_FORMATION_INDEX_GENERATED_UNIT;
    main_unit_init_from_entd(unit, 0, 0, 0x82);
    unit->x = x;
    unit->position.y = y;
    unit->formation_index = formation_index;
    unit->position.higher_elevation = higher_elevation;
    unit->position.facing = facing;
    main_mem_copy_byte_data_source_first(saved_name, name, 0x10);
    unit->unit_id = unit_id;
    main_unit_calculate_palette_spritesheet(unit, palette);
}

/* Apply a level-up or level-down special effect: level + 1 (99 at most) or
 * level - 1 (1 at least), then one level of growth or its inverse. The new
 * level is stored before the growth runs, so the growth divides by growth +
 * new level. The unit changes only when executing; returns 1 when the action
 * carries either effect. */
static s32 battle_unit_apply_level_up_down_ability(void) {
    s32 lv;
    s32 dir;
    u16 flags = g_battle_action_target_data->special_effect;

    if (flags & ACTION_SPECIAL_EFFECT_LEVEL_UP) {
        lv = g_battle_action_target->level + 1;
        dir = 0;
        if (lv > UNIT_LEVEL_MAX) {
            lv = UNIT_LEVEL_MAX;
        }
    } else if (flags & ACTION_SPECIAL_EFFECT_LEVEL_DOWN) {
        lv = g_battle_action_target->level - 1;
        dir = 1;
        if (lv <= 0) {
            lv = 1;
        }
    } else {
        return 0;
    }
    if (g_battle_action_state == ACTION_STATE_EXECUTE) {
        g_battle_action_target->level = lv;
        main_unit_apply_level_growth(g_battle_action_target, dir);
    }
    return 1;
}

/* Nonzero when the unit cannot react: a status or a non-primary action
 * prevents it (battle_action_is_reaction_prevented), a reaction is already in
 * play (battle_action_check_reaction), or it cannot act or stands in deep
 * water (battle_unit_get_action_restriction). Returns the first nonzero
 * result, or 0 when the unit can react. */
s32 battle_action_is_reaction_blocked(const unit_stats_t* unit) {
    s32 result;

    if ((result = battle_action_is_reaction_prevented(unit)) == 0
        && (result = battle_action_check_reaction(unit)) == 0) {
        result = battle_unit_get_action_restriction(unit);
    }
    return result;
}

/* Nonzero when the unit cannot react: it has a status of the prevent-reaction
 * set, or the current action is itself a reaction or a simulation (anything but
 * the primary context), which never provokes one. */
s32 battle_action_is_reaction_prevented(const unit_stats_t* unit) {
    if (g_battle_action_context != ACTION_CONTEXT_PRIMARY) {
        return 1;
    }
    return main_unit_has_status_in_set(unit, MAIN_STATUS_CHECK_SET_PREVENT_REACTION);
}

/* Whether a reaction is already in play: 1 while a weapon's proc ability is
 * being resolved, 2 when the unit's result already carries a reaction, 0
 * otherwise. */
static s32 battle_action_check_reaction(const unit_stats_t* unit) {
    s32 result;
    /* Both globals are loaded unsigned (lhu) here. */
    if (g_battle_action_current_ability.reaction_id != 0) {
        if (g_battle_action_current_ability.weapon_proc_pending == 0)
            return 1;
    }
    /* The target loads the reaction id signed (lh). */
    result = unit->action.reaction_id;
    if (result != 0)
        result = 2;
    return result;
}

/* Apply the target's reflect or weapon-evasion reactions before the attack resolves. */
static void battle_action_check_evasion_reactions(void) {
    u8 reaction_flags;

    if (g_battle_action_context != ACTION_CONTEXT_PRIMARY) {
        return;
    }
    if (g_battle_action_current_ability.formula == ABILITY_FORMULA_WEAPON_HEAL) {
        return;
    }
    if (battle_action_check_reaction(g_battle_action_target) == 1) {
        return;
    }
    if ((g_battle_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_REFLECT)]
            & STATUS_BYTE_MASK(STATUS_ID_REFLECT))
        && g_battle_action_current_ability.reaction_id == 0) {
        battle_action_check_reflect_reaction();
    }
    /* reaction_id is loaded signed (lh) here. */
    if (g_battle_action_target_data->reaction_id != 0) {
        return;
    }
    if (battle_action_is_reaction_prevented(g_battle_action_target) != 0) {
        return;
    }
    if (battle_unit_get_action_restriction(g_battle_action_target) != BATTLE_UNIT_ACTION_RESTRICTION_NONE) {
        return;
    }
    reaction_flags = g_battle_action_target->ability_flags.reaction[3];
    if (reaction_flags & BATTLE_REACTION_SET_4_BLADE_GRASP) {
        battle_action_check_blade_grasp_usability();
        return;
    }
    if (reaction_flags & BATTLE_REACTION_SET_4_ARROW_GUARD) {
        battle_action_check_arrow_guard_usability();
    }
}

/* Dispatches the target unit's first reaction ability whose bit is set in
 * ability_flags.reaction[0..2]. Formula 7 abilities never provoke a reaction.
 *
 * Counter Magic (byte 1) is tested last, after the byte-2 reactions, so
 * Absorb Used MP or Counter takes precedence over it. */
static void battle_action_dispatch_target_reaction_ability(void) {
    unit_stats_t* unit;

    if (g_battle_action_current_ability.formula == ABILITY_FORMULA_WEAPON_HEAL)
        return;
    if (battle_action_is_reaction_blocked(g_battle_action_target) != 0)
        return;
    unit = g_battle_action_target;
    if (unit->ability_flags.reaction[0] & BATTLE_REACTION_SET_1_SUNKEN_STATE) {
        battle_action_check_counter_reaction_usability(
            ABILITY_ID_REACTION_SUNKEN_STATE, ABILITY_SECONDARY_FLAG_4_BLADE_GRASP_ELIGIBLE);
    } else if (unit->ability_flags.reaction[0] & BATTLE_REACTION_SET_1_CAUTION) {
        battle_action_check_counter_reaction_usability(
            ABILITY_ID_REACTION_CAUTION, ABILITY_SECONDARY_FLAG_4_BLADE_GRASP_ELIGIBLE);
    } else if (unit->ability_flags.reaction[0] & BATTLE_REACTION_SET_1_DRAGON_SPIRIT) {
        battle_action_check_counter_reaction_usability(
            ABILITY_ID_REACTION_DRAGON_SPIRIT, ABILITY_SECONDARY_FLAG_4_BLADE_GRASP_ELIGIBLE);
    } else if (unit->ability_flags.reaction[0] & BATTLE_REACTION_SET_1_BRAVE_UP) {
        battle_action_check_counter_reaction_usability(
            ABILITY_ID_REACTION_BRAVE_UP, ABILITY_SECONDARY_FLAG_4_BLADE_GRASP_ELIGIBLE);
    } else if (unit->ability_flags.reaction[1] & BATTLE_REACTION_SET_2_FAITH_UP) {
        battle_action_check_mp_cost_reaction_usability(ABILITY_ID_REACTION_FAITH_UP);
    } else if (unit->ability_flags.reaction[1] & BATTLE_REACTION_SET_2_COUNTER_TACKLE) {
        battle_action_check_counter_reaction_usability(
            ABILITY_ID_REACTION_COUNTER_TACKLE, ABILITY_SECONDARY_FLAG_4_BLADE_GRASP_ELIGIBLE);
    } else if (unit->ability_flags.reaction[1] & BATTLE_REACTION_SET_2_COUNTER_FLOOD) {
        battle_action_check_counter_reaction_usability(
            ABILITY_ID_REACTION_COUNTER_FLOOD, ABILITY_SECONDARY_FLAG_4_COUNTER_FLOOD);
    } else if (unit->ability_flags.reaction[2] & BATTLE_REACTION_SET_3_ABSORB_USED_MP) {
        battle_action_check_mp_cost_reaction_usability(ABILITY_ID_REACTION_ABSORB_USED_MP);
    } else if (unit->ability_flags.reaction[2] & BATTLE_REACTION_SET_3_COUNTER) {
        battle_action_check_counter_reaction_usability(
            ABILITY_ID_REACTION_COUNTER, ABILITY_SECONDARY_FLAG_4_BLADE_GRASP_ELIGIBLE);
    } else if (unit->ability_flags.reaction[1] & BATTLE_REACTION_SET_2_COUNTER_MAGIC) {
        battle_action_check_counter_reaction_usability(
            ABILITY_ID_REACTION_COUNTER_MAGIC, ABILITY_SECONDARY_FLAG_4_COUNTER_MAGIC);
    }
}

/* Check the target's first equipped damage-sharing reaction, in the order MP
 * Switch, Distribute, Damage Split; when its check fails no other is tried.
 * Nothing runs for the Weapon Heal formula or a target that cannot react. */
static void battle_action_check_damage_sharing_reaction_usability(void) {
    unit_stats_t* unit;
    u8 flags;

    if (g_battle_action_current_ability.formula == ABILITY_FORMULA_WEAPON_HEAL)
        return;
    if (battle_action_is_reaction_blocked(g_battle_action_target) != 0)
        return;
    unit = g_battle_action_target;
    flags = unit->ability_flags.reaction[2];
    if (flags & BATTLE_REACTION_SET_3_MP_SWITCH) {
        battle_action_check_mp_switch_usability();
        return;
    }
    if (flags & BATTLE_REACTION_SET_3_DISTRIBUTE) {
        battle_action_check_distribute_usability();
        return;
    }
    if (unit->ability_flags.reaction[3] & BATTLE_REACTION_SET_4_DAMAGE_SPLIT) {
        battle_action_check_damage_split_usability();
    }
}

/* Dispatch the first active stat-save or resource-restoring reaction. A weapon
 * that heals never triggers them. */
static void battle_action_check_stat_save_and_restore_reaction_usability(void) {
    unit_stats_t* target;
    u8 reaction_flags;

    if (g_battle_action_current_ability.formula == ABILITY_FORMULA_WEAPON_HEAL
        || battle_action_is_reaction_blocked(g_battle_action_target) != 0) {
        return;
    }
    target = g_battle_action_target;
    reaction_flags = target->ability_flags.reaction[0];
    if (reaction_flags & BATTLE_REACTION_SET_1_PA_SAVE) {
        battle_action_check_hp_damage_reaction_usability(ABILITY_ID_REACTION_PA_SAVE);
    } else if (reaction_flags & BATTLE_REACTION_SET_1_MA_SAVE) {
        battle_action_check_hp_damage_reaction_usability(ABILITY_ID_REACTION_MA_SAVE);
    } else if (reaction_flags & BATTLE_REACTION_SET_1_SPEED_SAVE) {
        battle_action_check_hp_damage_reaction_usability(ABILITY_ID_REACTION_SPEED_SAVE);
    } else if (reaction_flags & BATTLE_REACTION_SET_1_REGENERATOR) {
        battle_action_check_hp_damage_reaction_usability(ABILITY_ID_REACTION_REGENERATOR);
    } else {
        reaction_flags = target->ability_flags.reaction[1];
        if (reaction_flags & BATTLE_REACTION_SET_2_HP_RESTORE) {
            battle_action_check_critical_hp_reaction_usability(ABILITY_ID_REACTION_HP_RESTORE);
        } else if (reaction_flags & BATTLE_REACTION_SET_2_MP_RESTORE) {
            battle_action_check_critical_hp_reaction_usability(ABILITY_ID_REACTION_MP_RESTORE);
        } else if (reaction_flags & BATTLE_REACTION_SET_2_CRITICAL_QUICK) {
            battle_action_check_critical_hp_reaction_usability(ABILITY_ID_REACTION_CRITICAL_QUICK);
        } else if (reaction_flags & BATTLE_REACTION_SET_2_MEATBONE_SLASH) {
            battle_action_check_critical_hp_reaction_usability(ABILITY_ID_REACTION_MEATBONE_SLASH);
        } else {
            reaction_flags = target->ability_flags.reaction[2];
            if (reaction_flags & BATTLE_REACTION_SET_3_GILGAME_HEART) {
                battle_action_check_hp_damage_reaction_usability(ABILITY_ID_REACTION_GILGAME_HEART);
            } else if (reaction_flags & BATTLE_REACTION_SET_3_AUTO_POTION) {
                battle_action_check_hp_damage_reaction_usability(ABILITY_ID_REACTION_AUTO_POTION);
            }
        }
    }
}

/* Arm a counter-type reaction on the target: the attack must not come through
 * Math Skill, its flags_4 must have a bit of mask, and the target's Brave roll
 * must pass. Records the reaction and the ability it answers. */
static void battle_action_check_counter_reaction_usability(u16 reaction_id, u32 mask) {
    action_data_t* action;

    if (g_battle_action_current_ability.skillset == SKILLSET_ID_MATH_SKILL)
        return;
    if ((g_battle_action_current_ability.range_data.flags_4 & mask) == 0)
        return;
    if (battle_action_calculate_chance_to_react(g_battle_action_target) != 0)
        return;
    action = g_battle_action_target_data;
    action->reaction_id = reaction_id;
    action->last_received_attack = g_battle_action_current_ability.ability_id;
}

/* Record reaction_id when the action deals HP damage and the target passes its
 * reaction roll; the HP damage is the reaction's amount. */
static void battle_action_check_hp_damage_reaction_usability(s16 reaction_id) {
    action_data_t* action;

    if ((g_battle_action_target_data->attack_type & ACTION_TYPE_HP_DAMAGE)
        && battle_action_calculate_chance_to_react(g_battle_action_target) == 0) {
        action = g_battle_action_target_data;
        action->reaction_id = reaction_id;
        action->last_received_attack = action->hp_damage;
    }
}

/* Record reaction_id for a target that is in Critical, is taking HP damage and
 * passes its reaction roll. */
static void battle_action_check_critical_hp_reaction_usability(s16 reaction_id) {
    unit_stats_t* unit = g_battle_action_target;

    if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRITICAL)] & STATUS_BYTE_MASK(STATUS_ID_CRITICAL))
        && (g_battle_action_target_data->attack_type & ACTION_TYPE_HP_DAMAGE)
        && battle_action_calculate_chance_to_react(unit) == 0) {
        g_battle_action_target_data->reaction_id = reaction_id;
    }
}

/* Record reaction_id when the ability cost MP and the target passes its reaction
 * roll; the MP cost is the reaction's amount. */
static void battle_action_check_mp_cost_reaction_usability(s16 reaction_id) {
    u8* used_mp = &g_battle_action_current_ability.mp_cost;
    action_data_t* action;
    if (*used_mp != 0) {
        if (battle_action_calculate_chance_to_react(g_battle_action_target) == 0) {
            action = g_battle_action_target_data;
            action->reaction_id = reaction_id;
            action->last_received_attack = *used_mp;
        }
    }
}

/* Reflect: a reflectable ability not cast through Math Skill misses the target
 * and is recorded on it for the bounce. */
static void battle_action_check_reflect_reaction(void) {
    action_data_t* action;
    /* Loaded through a pointer so the reaction_id store stays above it. */
    u16* ability_ptr = &g_battle_action_current_ability.ability_id;
    u16 ability;
    if (g_battle_action_current_ability.skillset != SKILLSET_ID_MATH_SKILL
        && (g_battle_action_current_ability.range_data.flags_3 & ABILITY_SECONDARY_FLAG_3_REFLECTABLE)) {
        action = g_battle_action_target_data;
        action->reaction_id = ABILITY_ID_REACTION_REFLECT;
        ability = *ability_ptr;
        /* The target stores the accuracy as a 16-bit value (0x2a..0x2b). */
        action->attack_accuracy = 0;
        action->hit = 0;
        action->last_received_attack = ability;
        g_battle_action_target_data->miss_type = ACTION_MISS_TYPE_REFLECTED;
    }
}

/* Blade Grasp. An ability flagged Blade Grasp eligible has its accuracy set to
 * 100 - Brave of the target; when the target passes its reaction roll during
 * execution the strike becomes a miss and the reaction is recorded. */
static void battle_action_check_blade_grasp_usability(void) {
    unit_stats_t* attacker;
    action_data_t* target;

    if ((g_battle_action_current_ability.range_data.flags_4 & ABILITY_SECONDARY_FLAG_4_BLADE_GRASP_ELIGIBLE) == 0)
        return;
    attacker = g_battle_action_target;
    target = g_battle_action_target_data;
    /* 16-bit store covers attack_accuracy and the pad byte after it. */
    target->attack_accuracy = 0x64 - attacker->brave;
    if (battle_action_calculate_chance_to_react(attacker) != 0)
        return;
    if (g_battle_action_state != ACTION_STATE_EXECUTE)
        return;
    g_battle_action_target_data->hit = 0;
    g_battle_action_target_data->miss_type = ACTION_MISS_TYPE_BLADE_GRASP_OR_FINGER_GUARD;
    g_battle_action_target_data->reaction_id = ABILITY_ID_REACTION_BLADE_GRASP;
}

/* Applies Arrow Guard to bow and crossbow attacks that use weapon range.
 *
 * A successful Brave-based reaction changes the target result to the shared
 * class-evade/Arrow Guard miss type. */
static void battle_action_check_arrow_guard_usability(void) {
    unit_stats_t* target;
    action_data_t* action;

    if ((g_battle_action_current_ability.range_data.flags_1 & ABILITY_SECONDARY_FLAG_1_WEAPON_RANGE) == 0) {
        return;
    }
    if (g_main_item_primary_data[g_battle_action_current_ability.weapon_id].type != ITEM_TYPE_CROSSBOW
        && g_main_item_primary_data[g_battle_action_current_ability.weapon_id].type != ITEM_TYPE_BOW) {
        return;
    }
    target = g_battle_action_target;
    action = g_battle_action_target_data;
    /* The accuracy field is written as a 16-bit value (sh) here. */
    action->attack_accuracy = 100 - target->brave;
    if (battle_action_calculate_chance_to_react(g_battle_action_target) == 0
        && g_battle_action_state == ACTION_STATE_EXECUTE) {
        g_battle_action_target_data->hit = 0;
        g_battle_action_target_data->miss_type = ACTION_MISS_TYPE_CLASS_EVADE_OR_ARROW_GUARD;
        g_battle_action_target_data->reaction_id = ABILITY_ID_REACTION_ARROW_GUARD;
    }
}

/* MP Switch: when the target would lose HP, still has MP and passes its reaction
 * roll, move the HP damage onto MP (capped at 999) and mark the reaction.
 *
 * Only an executing action (not a preview or simulation) is converted. */
static void battle_action_check_mp_switch_usability(void) {
    unit_stats_t* unit;
    action_data_t* action;
    action_data_t* action2;

    /* The target reads hp_damage / mp_damage signed (lh). */
    if (g_battle_action_target_data->hp_damage == 0) {
        return;
    }
    unit = g_battle_action_target;
    if (unit->mp == 0) {
        return;
    }
    if (battle_action_calculate_chance_to_react(g_battle_action_target) != 0) {
        return;
    }
    if (g_battle_action_state != ACTION_STATE_EXECUTE) {
        return;
    }
    action = g_battle_action_target_data;
    action->mp_damage = action->mp_damage + action->hp_damage;
    if (action->mp_damage >= 0x3E8) {
        action->mp_damage = 0x3E7;
    }
    action2 = g_battle_action_target_data;
    action2->attack_type &= ~ACTION_TYPE_HP_DAMAGE;
    action = g_battle_action_target_data;
    action2->hp_damage = 0;
    action->attack_type |= ACTION_TYPE_MP_DAMAGE;
    g_battle_action_target_data->reaction_id = ABILITY_ID_REACTION_MP_SWITCH;
}

/* Distribute: when the healing exceeds the target's missing HP and the reaction
 * roll passes, record the reaction with the excess as its amount. */
static void battle_action_check_distribute_usability(void) {
    unit_stats_t* target = g_battle_action_target;
    action_data_t* action = g_battle_action_target_data;
    /* hp_healing is read signed (lh) here. */
    s32 excess = action->hp_healing - (target->max_hp - target->hp);
    if (excess > 0 && battle_action_calculate_chance_to_react(target) == 0) {
        g_battle_action_target_data->last_received_attack = excess;
        g_battle_action_target_data->reaction_id = ABILITY_ID_REACTION_DISTRIBUTE;
    }
}

/* Damage Split: when the target takes HP damage and passes its reaction roll,
 * record the reaction with half the damage, rounded up, as its amount. */
static void battle_action_check_damage_split_usability(void) {
    u16 hp = g_battle_action_target_data->hp_damage;
    action_data_t* action;

    if (hp != 0 && battle_action_calculate_chance_to_react(g_battle_action_target) == 0) {
        hp++;
        hp = hp >> 1;
        action = g_battle_action_target_data;
        action->last_received_attack = hp;
        action->reaction_id = ABILITY_ID_REACTION_DAMAGE_SPLIT;
    }
}

/* Reaction roll: returns 0 when the unit reacts, a Brave in 100 chance.
 * Outside execution it always returns 0. */
s32 battle_action_calculate_chance_to_react(const unit_stats_t* unit) {
    if (g_battle_action_state != ACTION_STATE_EXECUTE) {
        return 0;
    }
    return main_util_roll_pass_fail(0x64, unit->brave);
}

static s32 battle_action_remove_broken_or_stolen_equipment(void) {
    s32 i;
    s32 count = 0;
    u8 item;
    unit_stats_t* unit;
    action_data_t* action;

    if (g_battle_action_target_data->special_effect
        & (ACTION_SPECIAL_EFFECT_BREAK_EQUIPMENT | ACTION_SPECIAL_EFFECT_STEAL_ITEM)) {
        for (i = 0; i < UNIT_EQUIPMENT_SLOT_COUNT; i++) {
            if (g_battle_action_target_data->equipment_destroyed & (ACTION_EQUIPMENT_SLOT_HEAD >> i)) {
                item = g_battle_action_target->equipment[i];
                if (item != ITEM_ID_NOTHING && item != ITEM_ID_NONE
                    && !(g_main_item_primary_data[item].type_flags & ITEM_TYPE_FLAG_UNREMOVABLE)) {
                    g_battle_action_target->equipment[i] = ITEM_ID_NONE;
                    count++;
                    if (g_battle_action_target_data->special_effect & ACTION_SPECIAL_EFFECT_STEAL_ITEM) {
                        battle_action_increment_item_quantity_for_steal_break(g_battle_action_attacker, item);
                    }
                }
            }
        }
        if (count != 0) {
            if (g_battle_action_state == ACTION_STATE_EXECUTE) {
                main_unit_refresh_stats_and_statuses(g_battle_action_target);
            }
            action = g_battle_action_target_data;
            if (action->equipment_destroyed == ACTION_EQUIPMENT_SLOT_RIGHT_WEAPON
                || action->equipment_destroyed == ACTION_EQUIPMENT_SLOT_LEFT_WEAPON) {
                unit = g_battle_action_target;
                if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHARGING)]
                        & STATUS_BYTE_MASK(STATUS_ID_CHARGING))
                    && unit->charged_ability_ct != 0xFF && unit->action_command.skillset_id == SKILLSET_ID_CHARGE) {
                    action->status_changes.removal[STATUS_BYTE_INDEX(STATUS_ID_CHARGING)]
                        |= STATUS_BYTE_MASK(STATUS_ID_CHARGING);
                }
            }
            return 1;
        }
        return 0;
    }
    return 0;
}

/* Apply one stat change of an action result: the low seven bits are the amount
 * and bit 0x80 means increase. The result is clamped to min..max; returns 1
 * when the stat changed. With max 255 (CT) an amount of 0x7f stands for 255,
 * which empties or fills CT. With min 1 (Speed, PA, MA) stat is the base
 * value: the total six bytes on is rebuilt as base + equipment bonus, capped
 * at max.
 *
 * The caller's ranges: Speed 1..50, PA and MA 1..99, Brave 0..100 (10 at least
 * for a rider), Faith 0..100, CT 0..255. */
static s32 battle_unit_apply_stat_increment_decrement(s32 mod, u8* stat, u8 max, u8 min) {
    s32 amount;
    s32 value = stat[0];

    amount = mod & ACTION_STAT_CHANGE_VALUE_MASK;
    if (max == 0xFF && amount == 0x7F) {
        amount = 0xFF;
    }

    if ((mod & ACTION_STAT_CHANGE_INCREASE) != 0) {
        value = value + amount;
    } else {
        value = value - amount;
    }

    if (value < min) {
        value = min;
    }
    if (value > max) {
        value = max;
    }

    if (value == stat[0]) {
        return 0;
    }
    stat[0] = value;

    if (min == 1) {
        value = stat[0] + stat[3];
        if (value > max) {
            value = max;
        }
        stat[6] = value;
    }
    return 1;
}

/* Record the highest EXP award produced by the current action.
 *
 * Success starts at 10; kills divide 20 by KO count. Level difference and
 * Gained EXP Up then adjust the result, which is clamped to 1 through 99. */
static void battle_unit_update_attacker_earned_experience(s32 outcome) {
    s16 base;
    s32 signed_exp;
    s32 divisor;
    unit_stats_t* attacker;
    s16 exp;
    s32 sum;

    if ((g_battle_action_context == ACTION_CONTEXT_PRIMARY) && (outcome != 0)) {
        if (outcome == 1) {
            base = 10;
        } else {
            divisor = g_battle_action_target->ko_count;
            base = 20;
            if (divisor != 0) {
                base = 20 / divisor;
            }
        }
        attacker = g_battle_action_attacker;
        sum = (base + g_battle_action_target->level) - attacker->level;
        exp = sum;
        if (exp <= 0) {
            exp = 1;
        }
        if (attacker->ability_flags.support[1] & BATTLE_SUPPORT_SET_2_GAINED_EXP_UP) {
            exp = exp * 2;
        }
        if (exp >= 100) {
            exp = 99;
        }
        signed_exp = exp << 16;
        if (g_battle_action_current_ability.earned_experience < (signed_exp >> 16)) {
            g_battle_action_current_ability.earned_experience = exp;
        }
    }
}

/* Apply the prepared action result to one target: make the unit the current
 * target, copy the 0x2c-byte result from g_battle_action_attacker_action_data
 * into its action record and finalize it with
 * action_finalize_attack_and_flag_reactions.
 *
 * Returns that routine's result, or -1 for an invalid or empty slot. A result
 * that is not a hit is marked as canceled. */
s32 battle_action_store_target_stats_pointer_data(s32 unit_id) {
    unit_stats_t* stats = &g_battle_unit_stats[unit_id];
    action_data_t* action;
    s32 result;

    g_battle_action_target = stats;
    if (unit_id >= BATTLE_UNIT_SLOT_COUNT) {
        return -1;
    }
    action = &g_battle_unit_stats[unit_id].action;
    g_battle_action_current_ability.target_id = unit_id;
    g_battle_action_target_data = action;
    if (stats->entd_slot == BATTLE_ENTD_SLOT_NONE) {
        return -1;
    }
    main_mem_copy_byte_data_source_first(&g_battle_action_attacker_action_data, action, 0x2C);
    result = battle_action_apply_target_result(unit_id);
    if (g_battle_action_target_data->hit == 0) {
        g_battle_action_target_data->miss_type = ACTION_MISS_TYPE_CANCELED;
    }
    return result;
}

/* Set up a unit's Poison or Regen tick as an action against itself: Poison deals MaxHP/8,
 * otherwise Regen heals MaxHP/8. Returns the action type, 0 when neither applies or the unit is
 * out of action. */
s32 battle_status_apply_poison_and_regen(unit_stats_t* unit) {
    u8 status;
    if (battle_status_is_unit_out_of_action(unit) != 0) {
        return 0;
    }
    battle_action_set_target_variables(unit);
    status = unit->status_sets.current[3];
    if (status & STATUS_BYTE_MASK(STATUS_ID_POISON)) {
        g_battle_action_target_data->hp_damage = unit->max_hp >> 3;
        g_battle_action_target_data->attack_type = ACTION_TYPE_HP_DAMAGE;
    } else if (status & STATUS_BYTE_MASK(STATUS_ID_REGEN)) {
        g_battle_action_target_data->hp_healing = unit->max_hp >> 3;
        g_battle_action_target_data->attack_type = ACTION_TYPE_HP_HEALING;
    }
    return g_battle_action_target_data->attack_type;
}

/* Between-turn step: makes a unit that has acted lose Transparent.
 *
 * The unit becomes the current target with a cleared action result. When it has
 * taken its action, carries Transparent as an inflicted status and has
 * transparent_removal_flag set, the result becomes a status change removing
 * Transparent. Returns the result's attack type, nonzero when there is a change
 * to show. Crystals, treasures, dead, petrified and jumping units return 0
 * untouched. */
s32 battle_status_remove_transparent(unit_stats_t* unit) {
    unit_stats_t* target;

    if (battle_status_is_unit_out_of_action(unit) != 0) {
        return 0;
    }
    battle_action_set_target_variables(unit);
    if (unit->action_taken != 0) {
        target = g_battle_action_target;
        if ((target->inflicted_status[STATUS_BYTE_INDEX(STATUS_ID_TRANSPARENT)]
                & STATUS_BYTE_MASK(STATUS_ID_TRANSPARENT))
            != 0) {
            if (target->transparent_removal_flag != 0) {
                g_battle_action_target_data->status_changes.removal[2] = STATUS_BYTE_MASK(STATUS_ID_TRANSPARENT);
                g_battle_action_target_data->attack_type = ACTION_TYPE_STATUS_CHANGE;
            }
        }
    }
    return g_battle_action_target_data->attack_type;
}

/* Count down the unit's timed statuses, Poison through Reflect (Death Sentence
 * is not among them), and queue the removal of each that runs out. */
void battle_status_update_expiring(s32 unit_id) {
    s32 i;
    s32 byte_idx;
    s32 mask;
    unit_stats_t* target;
    action_data_t* action;
    u8 remaining_ct;

    battle_action_set_target_variables(&g_battle_unit_stats[unit_id]);

    /* Statuses 24..38 (bytes 3 and 4): tick the CT of each inflicted,
       non-innate status and flag it for removal when it reaches zero. */
    for (i = 0; i < 15; i++) {
        byte_idx = (i / 8) + 3;
        target = g_battle_action_target;
        if (target->inflicted_status[byte_idx] & (mask = 0x80 >> (i & 7))) {
            if ((target->status_sets.innate[byte_idx] & mask) == 0) {
                remaining_ct = target->status_ct[i] - 1;
                target->status_ct[i] = remaining_ct;
                if (remaining_ct == 0) {
                    action = g_battle_action_target_data;
                    action->status_changes.removal[byte_idx] = mask | action->status_changes.removal[byte_idx];
                }
            }
        }
    }

    if (battle_status_modify_inflictions(0) != 0) {
        g_battle_action_target_data->attack_type = ACTION_TYPE_STATUS_CHANGE;
    }
}

/* Make the unit the current target and reset its action result record. */
static void battle_action_set_target_variables(unit_stats_t* unit) {
    action_data_t* action = &unit->action;
    g_battle_action_target = unit;
    g_battle_action_target_data = action;
    g_battle_action_current_ability.target_id = unit->battle_id;
    battle_action_clear_current_data(action);
}

/* 1 for an empty slot or a unit that is a crystal, dead, in the air from Jump,
 * petrified or a treasure; otherwise 0. */
static s32 battle_status_is_unit_out_of_action(const unit_stats_t* unit) {
    s32 result;
    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE)
        return 1;
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)]
        & (STATUS_BYTE_MASK(STATUS_ID_CRYSTAL) | STATUS_BYTE_MASK(STATUS_ID_DEAD) | STATUS_BYTE_MASK(STATUS_ID_JUMP)))
        return 1;
    result = unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_PETRIFY)]
        & (STATUS_BYTE_MASK(STATUS_ID_PETRIFY) | STATUS_BYTE_MASK(STATUS_ID_TREASURE));
    if (result != 0)
        result = 1;
    return result;
}

/* Prepare Poison Marsh's automatic status action for a grounded unit.
 *
 * Mounts cannot receive the terrain effect. A rider uses the linked mount's
 * Float status because that unit determines whether the pair touches the
 * terrain. The resident terrain table marks surfaces that inflict Poison. */
s32 battle_status_prepare_terrain_poison(unit_stats_t* unit) {
    s32 mount_info;
    u8 unit_id;
    action_data_t* action;

    mount_info = unit->mount_info;
    if (battle_status_is_unit_out_of_action(unit))
        return 0;
    if (mount_info & BATTLE_MOUNT_INFO_FLAG_MOUNT)
        return 0;
    if (mount_info & BATTLE_MOUNT_INFO_FLAG_RIDER) {
        if (g_battle_unit_stats[mount_info & BATTLE_MOUNT_INFO_PARTNER_ID_MASK]
                .status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FLOAT)]
            & STATUS_BYTE_MASK(STATUS_ID_FLOAT))
            return 0;
    } else if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FLOAT)] & STATUS_BYTE_MASK(STATUS_ID_FLOAT)) {
        return 0;
    }
    action = &unit->action;
    unit_id = unit->battle_id;
    g_battle_action_target = unit;
    g_battle_action_target_data = action;
    g_battle_action_current_ability.target_id = unit_id;
    battle_action_clear_current_data(action);
    if (g_main_terrain_status_flags[g_battle_map_tile_data[battle_map_calculate_location(unit)].surface.type]
        & MAIN_TERRAIN_STATUS_FLAG_POISON) {
        g_battle_action_target_data->status_changes.infliction[STATUS_BYTE_INDEX(STATUS_ID_POISON)]
            = STATUS_BYTE_MASK(STATUS_ID_POISON);
        if (battle_status_modify_inflictions(0)) {
            g_battle_action_target_data->attack_type = ACTION_TYPE_STATUS_CHANGE;
            return 1;
        }
    }
    return 0;
}

/* Set up the trap on a unit's tile as an action against it (against the rider when the unit is a
 * mount): Degenerator lowers a level, Deathtrap inflicts Death Sentence and Sleeping Gas inflicts
 * Sleep (a miss when they cannot take hold), Steel Needle deals MaxHP/5. Returns the trap id, or
 * BATTLE_TRAP_ID_NONE. */
s32 battle_action_apply_tile_trap(unit_stats_t* unit) {
    u8 trap = battle_map_get_move_find_result(unit->x, unit->position.y, unit->position.higher_elevation)->trap_id;
    action_data_t* action;

    if (unit->mount_info & BATTLE_MOUNT_INFO_FLAG_MOUNT) {
        unit = &g_battle_unit_stats[unit->mount_info & BATTLE_MOUNT_INFO_PARTNER_ID_MASK];
    }
    g_battle_action_current_ability.target_id = unit->battle_id;
    g_battle_action_target = unit;
    g_battle_action_target_data = &unit->action;
    battle_action_clear_current_data(&unit->action);
    switch (trap) {
    case BATTLE_TRAP_ID_DEGENERATOR:
        g_battle_action_target_data->attack_type = ACTION_TYPE_PSEUDO_STATUS;
        g_battle_action_target_data->special_effect = ACTION_SPECIAL_EFFECT_LEVEL_DOWN;
        break;
    case BATTLE_TRAP_ID_DEATHTRAP:
        g_battle_action_target_data->status_changes.infliction[STATUS_BYTE_INDEX(STATUS_ID_DEATH_SENTENCE)]
            = STATUS_BYTE_MASK(STATUS_ID_DEATH_SENTENCE);
        if (battle_status_modify_inflictions(0) != 0) {
            g_battle_action_target_data->attack_type = ACTION_TYPE_STATUS_CHANGE;
        } else {
            battle_formula_force_attack_miss();
        }
        break;
    case BATTLE_TRAP_ID_SLEEPING_GAS:
        g_battle_action_target_data->status_changes.infliction[STATUS_BYTE_INDEX(STATUS_ID_SLEEP)]
            = STATUS_BYTE_MASK(STATUS_ID_SLEEP);
        if (battle_status_modify_inflictions(0) != 0) {
            g_battle_action_target_data->attack_type = ACTION_TYPE_STATUS_CHANGE;
        } else {
            battle_formula_force_attack_miss();
        }
        break;
    case BATTLE_TRAP_ID_STEEL_NEEDLE:
        action = g_battle_action_target_data;
        action->hp_damage = unit->max_hp / 5;
        action->attack_type = ACTION_TYPE_HP_DAMAGE;
        break;
    default:
        trap = BATTLE_TRAP_ID_NONE;
        break;
    }
    return trap;
}

/* Prepare the action record for a movement ability's benefit.
 *
 * Initialize the record, then select HP, MP, EXP or JP in that order. Move-HP
 * Up and Move-MP Up restore a tenth of the maximum, rounded up; Move-Get EXP
 * and Move-Get JP give battle_unit_calculate_move_exp_jp_up_to_earn's amount,
 * doubled by Gained EXP Up or Gained JP Up. Return -1 for a blocking status
 * and 0 otherwise, including when no movement benefit is present. The caller
 * applies and displays the result. */
s32 battle_action_init_movement_ability_benefit(unit_stats_t* unit) {
    s32 flags;
    u8 unit_id = unit->battle_id;
    s32 amount;
    action_data_t* action;

    g_battle_action_target = unit;
    g_battle_action_target_data = &unit->action;
    g_battle_action_current_ability.target_id = unit_id;
    battle_action_clear_current_data(&unit->action);
    if (main_unit_has_status_in_set(unit, MAIN_STATUS_CHECK_SET_PREVENT_REACTION))
        return -1;
    flags = battle_move_get_support_flags(unit);
    if (flags & MOVE_POST_EVENT_MOVE_HP_UP) {
        amount = (unit->max_hp + 9) / 10;
        action = g_battle_action_target_data;
        action->attack_type = ACTION_TYPE_HP_HEALING;
        action->hp_healing = amount;
    } else if (flags & MOVE_POST_EVENT_MOVE_MP_UP) {
        amount = (unit->max_mp + 9) / 10;
        action = g_battle_action_target_data;
        action->attack_type = ACTION_TYPE_MP_HEALING;
        action->mp_healing = amount;
    } else if (flags & MOVE_POST_EVENT_MOVE_GET_EXP) {
        g_battle_action_target_data->exp_change = battle_unit_calculate_move_exp_jp_up_to_earn(g_battle_action_target);
        if (g_battle_action_target->ability_flags.support[1] & BATTLE_SUPPORT_SET_2_GAINED_EXP_UP)
            g_battle_action_target_data->exp_change *= 2;
        g_battle_action_target_data->attack_type = ACTION_TYPE_PSEUDO_STATUS;
    } else if (flags & MOVE_POST_EVENT_MOVE_GET_JP) {
        g_battle_action_target_data->jp_change = battle_unit_calculate_move_exp_jp_up_to_earn(g_battle_action_target);
        if (g_battle_action_target->ability_flags.support[1] & BATTLE_SUPPORT_SET_2_GAINED_JP_UP)
            g_battle_action_target_data->jp_change *= 2;
        g_battle_action_target_data->attack_type = ACTION_TYPE_PSEUDO_STATUS;
    }
    return 0;
}

/* Simulate the unit's pending action and write the EXP and JP gained and any
 * new level and job level to rewards[0..3]. Returns -1 for an empty unit
 * slot, otherwise the action finalization result. */
s32 battle_unit_build_gained_exp_jp_level_job_level(unit_stats_t* unit, action_reward_display_t* rewards) {
    s32 job_slot[2];
    s32 result;
    s32 unit_id;

    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
        return -1;
    }
    unit_id = unit->battle_id;
    g_battle_action_current_ability.level_gained_flag = 0;
    g_battle_action_current_ability.job_level_gained_flag = 0;
    g_battle_action_context = ACTION_CONTEXT_REACTION_OR_SIMULATION;
    g_battle_acting_unit_id = unit_id;
    result = battle_action_apply_target_result(unit->battle_id);
    g_battle_action_context = ACTION_CONTEXT_PRIMARY;
    rewards->earned_experience = unit->action.exp_change;
    rewards->earned_jp = unit->action.jp_change;
    rewards->level_for_display = 0;
    if (g_battle_action_current_ability.level_gained_flag != 0) {
        rewards->level_for_display = unit->level;
    }
    rewards->job_level_for_display = 0;
    if (g_battle_action_current_ability.job_level_gained_flag != 0) {
        rewards->job_level_for_display = battle_unit_load_job_level(unit, unit->job_id, job_slot);
    }
    return result;
}

/* Adds the pending JP change to the unit's job and, if that changed the job
   level, sets the job-level-gained flag so the caller queues the level-up report. */
static void battle_action_apply_jp_change(unit_stats_t* unit) {
    s32 jp;
    s32 initial_level;
    u8 job = unit->job_id;
    u8 battle_id;

    battle_id = unit->battle_id;
    initial_level = battle_unit_load_job_level(unit, job, &jp);
    battle_unit_store_jp_and_calculate_unlocked_jobs(
        battle_id, unit->action.jp_change, unit->initial_team_flags & BATTLE_TEAM_MASK, jp);
    if (battle_unit_load_job_level(unit, job, &jp) != initial_level) {
        g_battle_action_current_ability.job_level_gained_flag = 1;
    }
}

/* Apply the current target record's status changes to unit unit_id, making it the current target:
 * filter the inflictions, apply the removals and inflictions with their CTs, zero HP under Dead,
 * drop the charged ability CT unless it is still Charging, Jumping or Performing, and refresh
 * player control (Berserk, Confusion, Charm and the like). The record's status-change bytes are
 * restored afterward. -1 for an empty slot. */
s32 battle_status_resolve_unit_changes(s32 unit_id, s32 removal_only) {
    unit_action_status_changes_t saved;

    if (g_battle_unit_stats[unit_id].entd_slot == BATTLE_ENTD_SLOT_NONE) {
        return -1;
    }
    main_mem_copy_byte_data_source_first(&g_battle_action_target_data->status_changes, &saved, sizeof(saved));
    g_battle_action_current_ability.target_id = unit_id;
    g_battle_action_target = &g_battle_unit_stats[unit_id];
    g_battle_action_target_data = &g_battle_unit_stats[unit_id].action;
    battle_status_modify_inflictions(removal_only);
    battle_status_apply_action_removals(unit_id);
    battle_status_store_for_current_attack(unit_id, removal_only);
    if (g_battle_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DEAD)]
        & STATUS_BYTE_MASK(STATUS_ID_DEAD)) {
        g_battle_action_target->hp = 0;
    }
    if (!(g_battle_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHARGING)]
            & (STATUS_BYTE_MASK(STATUS_ID_CHARGING) | STATUS_BYTE_MASK(STATUS_ID_JUMP)
                | STATUS_BYTE_MASK(STATUS_ID_PERFORMING)))) {
        g_battle_action_target->charged_ability_ct = 0xFF;
    }
    battle_status_remove_control(g_battle_action_target);
    main_mem_copy_byte_data_source_first(&saved, &g_battle_action_target_data->status_changes, sizeof(saved));
    return 0;
}

/* Confusion, Blood Suck, Berserk, Chicken and Charm take the unit out of
 * player control; without them the control bit comes back from the initial
 * team flags. Without Charm the team bits come back from the initial team as
 * well. */
static void battle_status_remove_control(unit_stats_t* unit) {
    if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CONFUSION)]
            & (STATUS_BYTE_MASK(STATUS_ID_CONFUSION) | STATUS_BYTE_MASK(STATUS_ID_BLOOD_SUCK)))
        || (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_BERSERK)]
            & (STATUS_BYTE_MASK(STATUS_ID_BERSERK) | STATUS_BYTE_MASK(STATUS_ID_CHICKEN)))
        || (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHARM)] & STATUS_BYTE_MASK(STATUS_ID_CHARM))) {
        unit->team_flags = unit->team_flags & ~BATTLE_TEAM_FLAG_PLAYER_CONTROLLED;
    } else {
        unit->team_flags = unit->team_flags | (unit->initial_team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED);
    }
    if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHARM)] & STATUS_BYTE_MASK(STATUS_ID_CHARM)) == 0) {
        unit->team_flags = (unit->team_flags & ~BATTLE_TEAM_MASK) | (unit->initial_team_flags & BATTLE_TEAM_MASK);
    }
}

/* Apply the removals of the unit's action result: each removed status leaves
 * inflicted_status, has its CT cleared and, when executing, its graphics flag
 * switched off; then the current set is rebuilt as innate | inflicted. */
static void battle_status_apply_action_removals(s32 unit_idx) {
    unit_stats_t* unit = &g_battle_unit_stats[unit_idx];
    s32 i;
    s32 mask;
    s32 idx;

    for (i = 0; i < STATUS_COUNT; i++) {
        idx = i / 8;
        mask = 0x80 >> (i & 7);
        if (unit->action.status_changes.removal[idx] & mask) {
            unit->inflicted_status[idx] = unit->inflicted_status[idx] & ~mask;
            if (main_status_set_ct(unit, i, 1) == 0) {
                battle_status_queue_graphics_change_if_executing(STATUS_HANDLER_INDEX(i), 0, unit_idx);
            }
        }
    }
    main_status_store_current(unit);
}

/* Applies the pending status inflictions of unit_id's current action.
 *
 * For each of the 40 statuses the action inflicts that none of the unit's
 * inflicted statuses blocks, the status's cancel list becomes the action's
 * removal set, the pending changes are filtered, the status is recorded as
 * inflicted and given its CT, and its special flags are enabled (mode 2 when
 * re-inflicting anything but Crystal or Oil). Transparent clears the removal
 * flag; Invite and Charm copy the attacker's team, Invite permanently. A unit
 * that started on the blue team and becomes Crystal or Dead sets
 * g_battle_player_unit_fallen, which voids the battle's level bonus money.
 * Ends by storing the unit's current statuses. */
static void battle_status_store_for_current_attack(s32 unit_id, s32 removal_only) {
    s32 i;
    s32 j;
    s32 byte;
    u8 mask;
    s32 blocked;
    u8 previous;

    g_battle_action_current_ability.target_id = unit_id;
    g_battle_action_target = &g_battle_unit_stats[unit_id];
    g_battle_action_target_data = &g_battle_unit_stats[unit_id].action;
    for (i = 0; i < STATUS_COUNT; i++) {
        byte = i / 8;
        /* Signed shift (srav); STATUS_BYTE_MASK shifts unsigned. */
        mask = 0x80 >> (i & 7);
        if (!(g_battle_action_target_data->status_changes.infliction[byte] & mask)) {
            continue;
        }
        blocked = 0;
        for (j = 0; j < STATUS_BYTE_COUNT; j++) {
            if (g_battle_action_target->inflicted_status[j] & g_main_status_effect_data[i].cant_stack[j]) {
                blocked = 1;
                break;
            }
        }
        if (blocked != 0) {
            continue;
        }
        for (j = 0; j < STATUS_BYTE_COUNT; j++) {
            g_battle_action_target_data->status_changes.removal[j] = g_main_status_effect_data[i].cancels[j];
        }
        battle_status_modify_inflictions(removal_only);
        battle_status_apply_action_removals(unit_id);
        previous = g_battle_action_target->inflicted_status[byte];
        g_battle_action_target->inflicted_status[byte] = previous | mask;
        if (main_status_set_ct(g_battle_action_target, i, 0) != 0) {
            continue;
        }
        if ((previous & mask) && i != STATUS_ID_CRYSTAL && i != STATUS_ID_OIL) {
            battle_status_queue_graphics_change_if_executing(STATUS_HANDLER_INDEX(i), 2, unit_id);
        } else {
            battle_status_queue_graphics_change_if_executing(STATUS_HANDLER_INDEX(i), 1, unit_id);
        }
        if (i == STATUS_ID_TRANSPARENT) {
            g_battle_action_target->transparent_removal_flag = 0;
        }
        if (i == STATUS_ID_INVITE || i == STATUS_ID_CHARM) {
            previous = (g_battle_action_target->team_flags & ~BATTLE_TEAM_MASK)
                | (g_battle_action_attacker->team_flags & BATTLE_TEAM_MASK);
            g_battle_action_target->team_flags = previous;
            if (i == STATUS_ID_INVITE) {
                previous = (g_battle_action_target->initial_team_flags & ~BATTLE_TEAM_OR_PLAYER_CONTROL_MASK)
                    | (g_battle_action_attacker->team_flags & BATTLE_TEAM_MASK);
                g_battle_action_target_data->reaction_id = 0;
                g_battle_action_target->initial_team_flags = previous;
                g_battle_action_target->auto_battle_setting = AI_AUTOBATTLE_MANUAL;
                g_battle_action_target->auto_battle_target = 0;
            }
        }
        if (!(g_battle_action_target->initial_team_flags & BATTLE_TEAM_MASK) && (i == 1 || i == 2)) {
            g_battle_player_unit_fallen = 1;
        }
    }
    main_status_store_current(g_battle_action_target);
}
