#include "fft/battle.h"

void battle_formula_magical_damage(void) {
    if (battle_formula_calculate_magical_evade() == 0) {
        battle_formula_store_ma_and_y();
        battle_formula_apply_elemental_strengthen();
        battle_formula_apply_magical_support_status_compatibility();
        if (battle_formula_calculate_elemental_xa_times_ya() == 0) {
            battle_formula_calculate_faith();
            if (battle_formula_apply_elemental_absorption_and_status_proc() == 0) {
                battle_formula_apply_status();
            }
        }
    }
}
