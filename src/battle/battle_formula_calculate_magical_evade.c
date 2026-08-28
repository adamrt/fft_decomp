#include "fft/battle.h"

s32 battle_formula_calculate_magical_evade(void) {
    battle_formula_store_magical_evade_values();
    battle_formula_calculate_transparent();
    battle_formula_calculate_abandon();
    battle_formula_apply_evasion_changes_due_to_statuses();
    return battle_formula_calculate_hit();
}
