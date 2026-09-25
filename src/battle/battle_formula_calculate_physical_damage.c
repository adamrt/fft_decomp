#include "fft/battle.h"

void battle_formula_calculate_physical_damage(void) {
    battle_formula_apply_physical_xa_modifying_statuses_support();
    battle_formula_calculate_critical_hit();
    battle_formula_store_xa_times_ya_damage();
}
