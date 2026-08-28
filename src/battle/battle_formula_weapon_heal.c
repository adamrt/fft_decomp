#include "fft/battle.h"

void battle_formula_weapon_heal(void) {
    battle_formula_calculate_base_xa();
    battle_formula_apply_zodiac_compatibility();
    battle_formula_apply_charge();
    battle_formula_apply_physical_xa_modifying_statuses_support();
    battle_formula_store_xa_times_ya_damage();
    battle_formula_apply_undead_reversal();
}
