#include "fft/battle.h"

void battle_formula_magical_heal(void) {
    battle_formula_store_ma_and_y();
    battle_formula_apply_elemental_strengthen();
    battle_formula_calculate_magical_xa_times_ya();
    battle_formula_calculate_faith();
    battle_formula_apply_undead_reversal();
}
