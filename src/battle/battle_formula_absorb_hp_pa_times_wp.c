#include "psx/types.h"

void battle_formula_absorb_hp_pa_times_wp(void) {
    if (battle_formula_calculate_physical_evade() == 0) {
        battle_formula_store_pa_and_weapon_power();
        battle_formula_calculate_physical_damage();
        battle_formula_apply_hp_absorption();
    }
}
