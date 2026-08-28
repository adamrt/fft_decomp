#include "fft/battle.h"
#include "psx/types.h"

void battle_formula_apply_physical_xa_modifying_statuses_support(void) {
    battle_formula_apply_physical_attack_supports();
    battle_formula_apply_attacker_berserk_frog();
    battle_formula_apply_defense_up();
    battle_formula_apply_target_xa_affecting_statuses();
    battle_formula_apply_zodiac_compatibility();
}
