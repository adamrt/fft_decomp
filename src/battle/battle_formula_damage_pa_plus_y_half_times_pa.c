#include "fft/battle.h"
#include "psx/types.h"

void battle_formula_damage_pa_plus_y_half_times_pa(void) {
    if (battle_formula_calculate_physical_evade() == 0) {
        battle_formula_store_pa_and_pa_plus_y_divided_by_two();
        battle_formula_apply_elemental_strengthen();
        battle_formula_apply_attack_up_and_martial_arts();
        battle_formula_apply_physical_status_support_compatibility();
        battle_formula_calculate_critical_hit();
        battle_formula_store_xa_times_ya_damage();
        battle_formula_apply_weather_elemental_effects();
        battle_formula_apply_elemental();
        if (g_battle_action_target_data->hit != 0) {
            battle_formula_apply_elemental_absorption();
            if (battle_formula_roll_conditional_status_proc() == 0) {
                battle_formula_apply_status();
            }
        }
    }
}
