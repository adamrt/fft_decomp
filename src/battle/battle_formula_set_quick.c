#include "fft/battle.h"

void battle_formula_set_quick(void) {
    battle_formula_store_ma_and_x();
    if (battle_formula_calculate_friendly_magic_accuracy() == 0) {
        battle_formula_apply_quick_effect();
    }
}
