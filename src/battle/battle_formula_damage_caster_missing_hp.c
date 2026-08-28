#include "fft/battle.h"

void battle_formula_damage_caster_missing_hp(void) {
    if (battle_formula_calculate_physical_evade() == 0) {
        battle_formula_store_ma_and_x();
        if (battle_formula_calculate_physical_status_accuracy() == 0) {
            battle_formula_43_damage_caster_missing_hp();
        }
    }
}
