#include "fft/battle.h"

void battle_formula_calculate_truth_damage(void) {
    battle_formula_store_ma_and_ma_plus_y_divided_by_two();
    battle_formula_calculate_truth_magical_damage();
}
