#include "fft/battle.h"
#include "psx/types.h"

void battle_formula_damage_sp_times_wp(void) {
    battle_formula_apply_catch();
    if (g_battle_action_target_data->hit == 0) {
        return;
    }
    if (battle_formula_calculate_physical_evade() != 0) {
        return;
    }
    g_current_ability.xa = g_battle_action_attacker->attributes[UNIT_ATTRIBUTE_SPEED];
    g_current_ability.ya = g_current_ability.weapon_data.power;
    battle_formula_apply_physical_status_support_compatibility();
    battle_formula_store_xa_times_ya_damage();
    battle_formula_modify_elemental_damage();
    if (g_battle_action_target_data->hit == 0) {
        return;
    }
    battle_formula_apply_elemental_absorption();
}
