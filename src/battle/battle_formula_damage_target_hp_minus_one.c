#include "fft/battle.h"

void battle_formula_damage_target_hp_minus_one(void) {
    if (battle_formula_calculate_magical_evade() == 0) {
        if (battle_formula_calculate_magic_accuracy_no_elemental() == 0) {
            battle_formula_calculate_gravi2_damage();
        }
    }
}
