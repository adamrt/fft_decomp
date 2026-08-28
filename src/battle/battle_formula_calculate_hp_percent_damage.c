#include "fft/battle.h"

void battle_formula_calculate_hp_percent_damage(void) {
    battle_action_data_t* action;
    s32 damage;

    damage = (g_battle_action_target->max_hp * g_current_ability.range_data.y + 99) / 100;
    action = g_battle_action_target_data;
    action->attack_type |= BATTLE_ACTION_TYPE_HP_DAMAGE;
    action->hp_damage = damage;
}
