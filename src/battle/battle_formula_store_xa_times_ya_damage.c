#include "fft/battle.h"

void battle_formula_store_xa_times_ya_damage(void) {
    u32 damage;
    battle_action_data_t* action;

    damage = g_current_ability.xa * g_current_ability.ya;
    action = g_battle_action_target_data;
    action->attack_type = BATTLE_ACTION_TYPE_HP_DAMAGE;
    action->hp_damage = damage;
}
