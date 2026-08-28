#include "fft/battle.h"

void battle_formula_physical_mp_percent_damage(void) {
    if (battle_formula_calculate_physical_evade() == 0) {
        battle_formula_store_pa_and_y();
        if (battle_formula_calculate_physical_accuracy() == 0) {
            battle_formula_calculate_mp_percent_damage();
        }
    }
}
