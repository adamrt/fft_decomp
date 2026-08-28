#include "fft/battle.h"
#include "psx/types.h"

/* Formula 0x0D: heal (Y)% of max HP, hit F(MA+X)%. */
void battle_formula_heal_y_percent_faith(void) {
    battle_formula_store_ma_and_x();
    battle_formula_apply_elemental_strengthen();
    if (battle_formula_calculate_friendly_magic_accuracy() != 0) {
        return;
    }
    if (battle_formula_apply_status_and_check_undead() == 0) {
        return;
    }
    battle_formula_calculate_hp_percent_damage();
    battle_formula_apply_undead_reversal();
}
