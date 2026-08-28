#include "fft/battle.h"
#include "psx/types.h"

void battle_formula_apply_hp_absorption(void) {
    battle_action_data_t* action;
    battle_action_data_t* attacker;
    battle_action_data_t* action2;
    battle_action_data_t* action3;
    battle_action_data_t* attacker2;
    u16 amount;

    if (g_battle_action_target->status_sets.current[0] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_UNDEAD)) {
        battle_action_finalize_target_current_action();
        action = g_battle_action_target_data;
        if (action->hit != 0) {
            attacker = g_battle_action_attacker_data;
            attacker->hp_damage = action->hp_damage;
            attacker->attack_type = BATTLE_ACTION_TYPE_HP_DAMAGE;
            g_battle_action_attacker_data->hit = 1;
            action2 = g_battle_action_target_data;
            amount = action2->hp_damage;
            action2->hp_damage = 0;
            action2->attack_type = BATTLE_ACTION_TYPE_HP_HEALING;
            action2->hp_healing = amount;
        }
        return;
    }
    battle_action_finalize_target_current_action();
    action3 = g_battle_action_target_data;
    if (action3->hit != 0) {
        attacker2 = g_battle_action_attacker_data;
        attacker2->hp_healing = action3->hp_damage;
        attacker2->attack_type = BATTLE_ACTION_TYPE_HP_HEALING;
        g_battle_action_attacker_data->hit = 1;
        g_battle_action_target_data->attack_type = BATTLE_ACTION_TYPE_HP_DAMAGE;
    }
}
