#include "fft/battle.h"

void battle_formula_damage_random_y_times_pa(void) {
    if (battle_formula_calculate_physical_evade() == 0) {
        battle_formula_store_pa_and_y();
        battle_formula_apply_attack_up_and_martial_arts();
        battle_formula_apply_physical_status_support_compatibility();
        battle_formula_apply_damage_and_knockback();
    }
}
