#include "fft/battle.h"

/* Formula 0x4C: heal (MA * Y) HP. */
void battle_formula_heal_ma_times_y(void) {
    battle_formula_store_ma_and_y();
    battle_formula_calculate_magical_xa_times_ya();
    battle_formula_apply_undead_reversal();
}
