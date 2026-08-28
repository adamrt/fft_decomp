#include "fft/battle.h"

void battle_formula_weapon_damage_charge(void) {
    if (battle_formula_calculate_physical_evade_charge() == 0) {
        battle_formula_calculate_base_xa();
        battle_formula_apply_charge();
        battle_formula_calculate_physical_damage();
        if (battle_formula_roll_conditional_status_proc() == 0) {
            battle_formula_apply_status();
        }
    }
}
