#include "fft/battle_formula.h"

/* Formula 0x0B: status, hit F(MA+X)%; no evade, and none of the target's magic
 * defenses. */
void formula_friendly_magic_accuracy(void) {
    formula_store_ma_and_x();
    formula_apply_elemental_strengthen();
    if (formula_calculate_friendly_magic_accuracy() == 0) {
        formula_apply_status_to_action();
    }
}
