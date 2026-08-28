#include "fft/battle.h"
#include "psx/types.h"

void battle_formula_calculate_mp_percent_damage(void) {
    battle_action_data_t* action;
    s32 damage;

    damage = (g_battle_action_target->max_mp * g_current_ability.range_data.y + 99) / 100;
    action = g_battle_action_target_data;
    action->attack_type = BATTLE_ACTION_TYPE_MP_DAMAGE;
    action->mp_damage = damage;
}
