#include "fft/battle.h"
#include "psx/types.h"

/* Formula 0x4D: absorb (Y)% of max HP, hit (MA+X)%. */
void battle_formula_absorb_hp_y_percent_hit_ma_x_percent(void) {
    if (battle_formula_calculate_physical_evade() != 0) {
        return;
    }
    battle_formula_store_ma_and_x();
    if (battle_formula_calculate_physical_status_accuracy() != 0) {
        return;
    }
    battle_formula_calculate_hp_percent_damage();
    battle_formula_apply_hp_absorption();
}
