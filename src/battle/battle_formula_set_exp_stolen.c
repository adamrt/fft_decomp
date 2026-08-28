#include "fft/battle.h"

/* volatile views: the target reloads these globals at every use. */
extern battle_stats_t* volatile g_battle_action_attacker;
extern battle_stats_t* volatile g_battle_action_target;

/*
 * Record the stolen EXP on both action results.
 *
 * The target's loss is encoded by adding 0x80 to the amount.
 */
void battle_formula_set_exp_stolen(void) {
    u8 amount;
    u8 available_exp;

    amount = g_battle_action_attacker->attributes[UNIT_ATTRIBUTE_SPEED] + g_current_ability.range_data.y;
    if (amount > 100) {
        amount = 100;
    }
    available_exp = g_battle_action_target->experience;
    if (available_exp < amount) {
        amount = available_exp;
    }
    if (amount == 0) {
        battle_formula_force_attack_miss();
        return;
    }
    g_battle_action_attacker_data->exp_change = amount;
    g_battle_action_attacker_data->hit = 1;
    g_battle_action_attacker_data->attack_type = 1;
    g_battle_action_target_data->exp_change = amount + 0x80;
    g_battle_action_target_data->attack_type = 1;
}
