#include "fft/battle.h"

void battle_formula_weapon_absorb_hp(void) {
    if (battle_formula_calculate_physical_evade() == 0) {
        battle_formula_calculate_base_xa();
        battle_formula_apply_charge();
        battle_formula_calculate_physical_damage();
        battle_formula_apply_hp_absorption();
    }
}
