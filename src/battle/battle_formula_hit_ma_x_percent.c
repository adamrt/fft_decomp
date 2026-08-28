#include "fft/battle.h"

void battle_formula_hit_ma_x_percent(void) {
    if (battle_formula_calculate_physical_evade() == 0) {
        battle_formula_store_ma_and_x();
        if (battle_formula_calculate_physical_status_accuracy() == 0) {
            battle_formula_apply_status_to_action();
        }
    }
}
