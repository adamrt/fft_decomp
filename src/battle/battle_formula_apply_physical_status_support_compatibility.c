#include "fft/battle.h"

void battle_formula_apply_physical_status_support_compatibility(void) {
    battle_formula_apply_attacker_berserk_frog();
    battle_formula_apply_defense_up();
    battle_formula_apply_target_xa_affecting_statuses();
    battle_formula_apply_zodiac_compatibility();
}
