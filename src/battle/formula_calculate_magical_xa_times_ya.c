#include "fft/battle_formula.h"

/* XA * YA with Magic Attack Up and zodiac compatibility only: the healing
 * formulas ignore the target's defenses. */
void formula_calculate_magical_xa_times_ya(void) {
    formula_apply_magic_attack_up();
    formula_apply_zodiac_compatibility();
    formula_store_xa_times_ya_damage();
}
