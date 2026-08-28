#include "fft/battle.h"
#include "psx/types.h"

s32 battle_formula_calculate_weapon_damage(void) {
    battle_formula_calculate_base_xa();
    battle_formula_apply_charge();
    battle_formula_apply_weapon_element_strengthen();
    battle_formula_calculate_physical_damage();
    battle_formula_modify_elemental_damage();
    if (g_battle_action_target_data->hit == 0)
        return 1;
    battle_formula_apply_elemental_absorption();
    return battle_formula_roll_conditional_status_proc();
}
