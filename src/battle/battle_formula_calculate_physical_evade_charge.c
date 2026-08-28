#include "fft/battle.h"
#include "psx/types.h"

s32 battle_formula_calculate_physical_evade_charge(void) {
    battle_formula_store_physical_evade_values();
    battle_formula_calculate_concentrate();
    battle_formula_calculate_dark_confuse();
    battle_formula_calculate_abandon();
    battle_formula_apply_evasion_changes_due_to_statuses();
    battle_formula_calculate_facing_evade();
    return battle_formula_calculate_hit();
}
