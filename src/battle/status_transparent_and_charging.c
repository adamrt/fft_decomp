/* Status transparent and charging. Its start is a cut chosen by topic; its end is a cut chosen by topic. */

#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_formula.h"
#include "fft/battle_move.h"
#include "fft/battle_status.h"
#include "psx/types.h"

/* Makes the attacker's own action result remove Transparent when its last
 * skillset is Jump and it carries Transparent as an inflicted status. Also points
 * g_action_attacker_data at the attacker's result. */
void status_remove_transparent_if_jump_used(unit_stats_t* unit) {
    action_data_t* action = &g_action_attacker_action_data;
    g_action_attacker_data = action;
    /* Inflicted status byte 2 bit 0x10 is Transparent. */
    if (unit->action_command.skillset_id == SKILLSET_ID_JUMP
        && (unit->inflicted_status[STATUS_BYTE_INDEX(STATUS_ID_TRANSPARENT)]
            & STATUS_BYTE_MASK(STATUS_ID_TRANSPARENT))) {
        action->hit = 1;
        g_action_attacker_action_data.attack_type |= ACTION_TYPE_STATUS_CHANGE;
        g_action_attacker_action_data.status_changes.removal[2]
            = g_action_attacker_action_data.status_changes.removal[2] | STATUS_BYTE_MASK(STATUS_ID_TRANSPARENT);
    }
}

/* Apply the ability's status. Returns 1 when it changes something or the
 * target is undead; otherwise the action is canceled as a forced failure and
 * 0 returned. */
s32 formula_apply_status_and_check_undead(void) {
    formula_apply_status();
    if (g_action_target_data->attack_type & ACTION_TYPE_STATUS_CHANGE) {
        return 1;
    }
    if (g_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_UNDEAD)]
        & STATUS_BYTE_MASK(STATUS_ID_UNDEAD)) {
        return 1;
    }
    formula_nullify_action();
    g_action_target_data->miss_type = ACTION_MISS_TYPE_FORCED_FAILURE;
    return 0;
}

/* Re-run the pre-formula setup against the current target as a plain weapon
 * Attack, then restore the attacker's skillset and ability and the strike
 * counter. It ends with the current ability's weapon_proc_pending and reaction_id
 * cleared. */
void action_switch_ability_to_default_attack(void) {
    unit_stats_t* unit;
    unit_stats_t* u2;
    unit_stats_t* u3;
    u8 saved_strike_counter;
    u8 saved_skillset;
    u16 saved_ability;

    unit = g_action_attacker;
    saved_strike_counter = g_action_current_ability.strike_counter;
    saved_skillset = unit->action_command.skillset_id;
    unit->action_command.skillset_id = SKILLSET_ID_ATTACK;
    u2 = g_action_attacker;
    saved_ability = unit->action_command.ability_id;
    u2->action_command.ability_id = 0;
    action_run_pre_formula_setup(&u2->action_command, g_action_current_ability.target_id);
    u3 = g_action_attacker;
    g_action_current_ability.strike_counter = saved_strike_counter;
    u3->action_command.ability_id = saved_ability;
    u3->action_command.skillset_id = saved_skillset;
    g_action_current_ability.weapon_proc_pending = 0;
    g_action_current_ability.reaction_id = 0;
}

/* Reset the result records of the current target and the current attacker; the
 * attacker's starts as not hit. */
void action_clear_data(void) {
    action_clear_current_data(g_action_target_data);
    action_clear_current_data(g_action_attacker_data);
    g_action_attacker_data->hit = 0;
}

/* Resolve a pending knockback as the target's own action. A grounded target
 * that is not a crystal, dead or petrified takes (fall height / 2 - Jump) *
 * MaxHP / 10 fall damage when that is positive, at most 999; a flying one
 * takes none. */
void move_apply_knockback(void) {
    u8* flags;
    unit_stats_t* unit;
    action_data_t* action;
    s32 amount;
    s32 diff;

    action_clear_data();
    flags = &g_action_current_ability.knockback_flags;
    *flags &= BATTLE_KNOCKBACK_KIND_MASK;
    g_action_target_data->attack_type = ACTION_TYPE_PSEUDO_STATUS;
    unit = g_action_target;
    amount = 0;
    if (!(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)] & STATUS_BYTE_MASK(STATUS_ID_CRYSTAL))
        && !(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DEAD)] & STATUS_BYTE_MASK(STATUS_ID_DEAD))
        && !(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_PETRIFY)] & STATUS_BYTE_MASK(STATUS_ID_PETRIFY))) {
        if (*flags == BATTLE_KNOCKBACK_FALL_DAMAGE) {
            diff = (g_action_current_ability.knockback_fall_height >> 1) - unit->jump;
            if (diff > 0) {
                amount = (diff * unit->max_hp) / 10;
                if (amount >= 0x3E8) {
                    amount = 0x3E7;
                }
            }
        }
        action = g_action_target_data;
        action->hp_damage = amount;
        if (amount != 0) {
            action->attack_type |= ACTION_TYPE_HP_DAMAGE;
        }
    }
}

/* Applies the pending reaction ability's effect to the current target action
 * record and finishes the strike. Returns 0 when the ability's behavior
 * flags select the alternate handler, -1 for an ability with no effect entry,
 * and 1 once the action has been finalised.
 *
 * The Damage Split case chains the damage store so the value reaches `healing` through
 * the target record; storing it there first is what orders the three global
 * pointer loads the way the target emits them. */
s32 action_perform_reaction_ability(void) {
    s16 ability_id;
    u8 attack_type;
    s32 healing;
    s32 count;

    attack_type = ACTION_TYPE_PSEUDO_STATUS;
    ability_id = g_battle_current_reaction_ability_id;
    if (ability_id == ABILITY_ID_REACTION_REFLECT) {
        g_action_attacker = &g_battle_unit_stats[g_battle_acting_unit_id];
    }
    if ((g_main_reaction_behavior_flags[ability_id - ABILITY_ID_REACTION_FIRST]
            & (BATTLE_REACTION_BEHAVIOR_FLAG_TRIGGER_ABILITY | BATTLE_REACTION_BEHAVIOR_FLAG_ABILITY))
        == 1) {
        return 0;
    }
    action_clear_data();
    switch (g_battle_current_reaction_ability_id) {
    case ABILITY_ID_REACTION_PA_SAVE:
        g_action_target_data->pa_change = 0x81;
        break;
    case ABILITY_ID_REACTION_MA_SAVE:
        g_action_target_data->ma_change = 0x81;
        break;
    case ABILITY_ID_REACTION_SPEED_SAVE:
        g_action_target_data->sp_change = 0x81;
        break;
    case ABILITY_ID_REACTION_SUNKEN_STATE:
        g_action_target_data->status_changes.infliction[STATUS_BYTE_INDEX(STATUS_ID_TRANSPARENT)]
            = STATUS_BYTE_MASK(STATUS_ID_TRANSPARENT);
        if (status_modify_inflictions(0) != 0) {
            attack_type = ACTION_TYPE_STATUS_CHANGE;
        }
        break;
    case ABILITY_ID_REACTION_CAUTION:
        g_action_target_data->status_changes.infliction[STATUS_BYTE_INDEX(STATUS_ID_DEFENDING)]
            = STATUS_BYTE_MASK(STATUS_ID_DEFENDING);
        if (status_modify_inflictions(0) != 0) {
            attack_type = ACTION_TYPE_STATUS_CHANGE;
        }
        break;
    case ABILITY_ID_REACTION_DRAGON_SPIRIT:
        g_action_target_data->status_changes.infliction[STATUS_BYTE_INDEX(STATUS_ID_RERAISE)]
            = STATUS_BYTE_MASK(STATUS_ID_RERAISE);
        if (status_modify_inflictions(0) != 0) {
            attack_type = ACTION_TYPE_STATUS_CHANGE;
        }
        break;
    case ABILITY_ID_REACTION_REGENERATOR:
        g_action_target_data->status_changes.infliction[STATUS_BYTE_INDEX(STATUS_ID_REGEN)]
            = STATUS_BYTE_MASK(STATUS_ID_REGEN);
        if (status_modify_inflictions(0) != 0) {
            attack_type = ACTION_TYPE_STATUS_CHANGE;
        }
        break;
    case ABILITY_ID_REACTION_BRAVE_UP:
        g_action_target_data->brave_change = 0x83;
        break;
    case ABILITY_ID_REACTION_FAITH_UP:
        g_action_target_data->faith_change = 0x83;
        break;
    case ABILITY_ID_REACTION_HP_RESTORE:
        attack_type = ACTION_TYPE_HP_HEALING;
        g_action_target_data->hp_healing = g_action_target->max_hp - g_action_target->hp;
        break;
    case ABILITY_ID_REACTION_MP_RESTORE:
        attack_type = ACTION_TYPE_MP_HEALING;
        g_action_target_data->mp_healing = g_action_target->max_mp - g_action_target->mp;
        break;
    case ABILITY_ID_REACTION_CRITICAL_QUICK:
        g_action_target_data->ct_change = ACTION_CT_CHANGE_QUICK;
        break;
    case ABILITY_ID_REACTION_MEATBONE_SLASH:
        attack_type = ACTION_TYPE_HP_DAMAGE;
        g_action_target_data->hp_damage = g_action_attacker->max_hp;
        break;
    case ABILITY_ID_REACTION_ABSORB_USED_MP:
        attack_type = ACTION_TYPE_MP_HEALING;
        g_action_target_data->mp_healing = g_action_target_data->last_received_attack;
        break;
    case ABILITY_ID_REACTION_GILGAME_HEART:
        g_action_target_data->gil_change = g_action_target_data->last_received_attack;
        break;
    case ABILITY_ID_REACTION_DISTRIBUTE:
        count = g_battle_distribute_target_count;
        if (count != 0) {
            healing = (g_action_attacker->action.last_received_attack + count - 1) / count;
        } else {
            healing = 0;
        }
        attack_type = ACTION_TYPE_HP_HEALING;
        g_action_target_data->hp_healing = healing;
        break;
    case ABILITY_ID_REACTION_DAMAGE_SPLIT:
        healing = g_action_target_data->hp_damage = g_action_attacker->action.last_received_attack;
        attack_type = ACTION_TYPE_HP_DAMAGE;
        g_action_attacker_data->hp_healing = healing;
        g_action_attacker_data->attack_type = ACTION_TYPE_HP_HEALING;
        g_action_attacker_data->hit = 1;
        break;
    default:
        g_action_target_data->attack_type = 0;
        return -1;
    }
    g_action_target_data->attack_type = attack_type;
    action_finalize_target_current_action();
    if (g_action_target_data->special_effect != 0) {
        g_action_target_data->attack_type |= ACTION_TYPE_PSEUDO_STATUS;
    }
    g_action_current_ability.reaction_id = 0;
    return 1;
}
