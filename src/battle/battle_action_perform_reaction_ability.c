#include "fft/battle.h"

/* Applies the pending reaction ability's effect to the current target action
 * record and finishes the strike. Returns 0 when the ability's behaviour
 * flags select the alternate handler, -1 for an ability with no effect entry,
 * and 1 once the action has been finalised.
 *
 * Case 0x18 chains the damage store so the value reaches `healing` through
 * the target record; storing it there first is what orders the three global
 * pointer loads the way the target emits them. */
s32 battle_action_perform_reaction_ability(void) {
    s16 ability_id;
    u8 attack_type;
    s32 healing;
    s32 selector;
    s32 count;

    attack_type = BATTLE_ACTION_TYPE_PSEUDO_STATUS;
    ability_id = g_battle_current_reaction_ability_id;
    if (ability_id == ABILITY_ID_REACTION_REFLECT) {
        g_battle_action_attacker = &g_battle_unit_stats[g_battle_acting_unit_id];
    }
    if ((g_main_reaction_behavior_flags_by_ability_id[ability_id] & 3) == 1) {
        return 0;
    }
    battle_action_clear_data();
    selector = (u16)g_battle_current_reaction_ability_id;
    switch ((s16)(selector - 0x1A6)) {
    case 0x0:
        g_battle_action_target_data->pa_change = 0x81;
        break;
    case 0x1:
        g_battle_action_target_data->ma_change = 0x81;
        break;
    case 0x2:
        g_battle_action_target_data->sp_change = 0x81;
        break;
    case 0x3:
        g_battle_action_target_data->status_infliction[2] = 0x10;
        if (battle_status_modify_inflictions(0) != 0) {
            attack_type = BATTLE_ACTION_TYPE_STATUS_CHANGE;
        }
        break;
    case 0x4:
        g_battle_action_target_data->status_infliction[0] = 2;
        if (battle_status_modify_inflictions(0) != 0) {
            attack_type = BATTLE_ACTION_TYPE_STATUS_CHANGE;
        }
        break;
    case 0x5:
        g_battle_action_target_data->status_infliction[2] = 0x20;
        if (battle_status_modify_inflictions(0) != 0) {
            attack_type = BATTLE_ACTION_TYPE_STATUS_CHANGE;
        }
        break;
    case 0x6:
        g_battle_action_target_data->status_infliction[3] = 0x40;
        if (battle_status_modify_inflictions(0) != 0) {
            attack_type = BATTLE_ACTION_TYPE_STATUS_CHANGE;
        }
        break;
    case 0x7:
        g_battle_action_target_data->brave_change = 0x83;
        break;
    case 0x8:
        g_battle_action_target_data->faith_change = 0x83;
        break;
    case 0x9:
        attack_type = BATTLE_ACTION_TYPE_HP_HEALING;
        g_battle_action_target_data->hp_healing = g_battle_action_target->max_hp - g_battle_action_target->hp;
        break;
    case 0xA:
        attack_type = BATTLE_ACTION_TYPE_MP_HEALING;
        g_battle_action_target_data->mp_healing = g_battle_action_target->max_mp - g_battle_action_target->mp;
        break;
    case 0xB:
        g_battle_action_target_data->ct_change = 0xFF;
        break;
    case 0xC:
        attack_type = BATTLE_ACTION_TYPE_HP_DAMAGE;
        g_battle_action_target_data->hp_damage = g_battle_action_attacker->max_hp;
        break;
    case 0x10:
        attack_type = BATTLE_ACTION_TYPE_MP_HEALING;
        g_battle_action_target_data->mp_healing = g_battle_action_target_data->last_received_attack;
        break;
    case 0x11:
        g_battle_action_target_data->gil_change = g_battle_action_target_data->last_received_attack;
        break;
    case 0x16:
        count = g_battle_distribute_target_count;
        if (count != 0) {
            healing = (g_battle_action_attacker->action.last_received_attack + count - 1) / count;
        } else {
            healing = 0;
        }
        attack_type = BATTLE_ACTION_TYPE_HP_HEALING;
        g_battle_action_target_data->hp_healing = healing;
        break;
    case 0x18:
        healing = g_battle_action_target_data->hp_damage = g_battle_action_attacker->action.last_received_attack;
        attack_type = BATTLE_ACTION_TYPE_HP_DAMAGE;
        g_battle_action_attacker_data->hp_healing = healing;
        g_battle_action_attacker_data->attack_type = BATTLE_ACTION_TYPE_HP_HEALING;
        g_battle_action_attacker_data->hit = 1;
        break;
    default:
        g_battle_action_target_data->attack_type = 0;
        return -1;
    }
    g_battle_action_target_data->attack_type = attack_type;
    battle_action_finalize_target_current_action();
    if (g_battle_action_target_data->special_effect != 0) {
        g_battle_action_target_data->attack_type |= BATTLE_ACTION_TYPE_PSEUDO_STATUS;
    }
    g_current_ability.reaction_id = 0;
    return 1;
}
