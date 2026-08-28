#include "fft/battle.h"
void battle_formula_magic_hp_absorb(void) {
    if (battle_formula_calculate_magical_evade() == 0) {
        if (battle_formula_calculate_magic_accuracy_no_elemental() == 0) {
            battle_formula_calculate_hp_percent_damage();
            battle_formula_apply_hp_absorption();
        }
    }
}
