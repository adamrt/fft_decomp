#include "fft/battle.h"

void battle_formula_ma_pa_half_damage(void) {
    if (battle_formula_calculate_magical_evade() == 0) {
        battle_formula_store_ma_and_pa_plus_y_divided_by_two();
        battle_formula_calculate_truth_magical_damage();
    }
}
