#include "fft/battle.h"
#include "psx/types.h"

/* Formula 0x3E: damage equal to the target's max HP - 1. */
void battle_formula_damage_target_max_hp_minus_one(void) {
    battle_formula_calculate_gravi2_damage();
}
