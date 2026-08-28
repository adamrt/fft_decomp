#include "fft/battle.h"

void battle_formula_lower_brave_y(void) {
    if (battle_formula_calculate_magical_evade() == 0) {
        if (battle_formula_calculate_magic_accuracy() == 0) {
            battle_formula_apply_y_brave();
        }
    }
}
