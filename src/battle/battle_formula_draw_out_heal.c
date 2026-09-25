#include "fft/battle.h"

void battle_formula_draw_out_heal(void) {
    battle_formula_calculate_katana_break_chance();
    battle_formula_store_ma_and_y();
    battle_formula_store_xa_times_ya_damage();
    battle_formula_apply_undead_reversal();
}
