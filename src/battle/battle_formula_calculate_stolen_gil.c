#include "fft/battle.h"
#include "fft/script_variables.h"
#include "psx/types.h"

void battle_formula_calculate_stolen_gil(void) {
    battle_stats_t* attacker;
    battle_action_data_t* action;
    battle_action_data_t* target_action;
    battle_action_data_t* other;
    s32 amount;
    s32 available;
    s32 stolen;

    attacker = g_battle_action_attacker;
    amount = attacker->attributes[UNIT_ATTRIBUTE_SPEED] * attacker->level;
    if (!(g_battle_action_target->team_flags & BATTLE_TEAM_MASK)) {
        available = battle_script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS);
        if (available < amount) {
            amount = available;
        }
    }
    if (amount == 0) {
        battle_formula_force_attack_miss();
        return;
    }
    action = g_battle_action_attacker_data;
    action->hit = 1;
    other = g_battle_action_attacker_data;
    action->gil_change = (s16)amount;
    other->attack_type = BATTLE_ACTION_TYPE_PSEUDO_STATUS;
    /* Read back unsigned (lhu), as the target does. */
    stolen = *(u16*)&g_battle_action_attacker_data->gil_change;
    target_action = g_battle_action_target_data;
    target_action->attack_type = BATTLE_ACTION_TYPE_PSEUDO_STATUS;
    target_action->gil_change = (s16)(0 - stolen);
}
