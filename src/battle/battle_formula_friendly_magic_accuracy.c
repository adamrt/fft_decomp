#include "fft/battle.h"

void battle_formula_friendly_magic_accuracy(void) {
    battle_formula_store_ma_and_x();
    battle_formula_apply_elemental_strengthen();
    if (battle_formula_calculate_friendly_magic_accuracy() == 0) {
        battle_formula_apply_status_to_action();
    }
}
