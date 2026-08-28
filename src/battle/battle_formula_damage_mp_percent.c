#include "fft/battle.h"

void battle_formula_damage_mp_percent(void) {
    if (battle_formula_calculate_magical_evade() == 0) {
        if (battle_formula_calculate_magic_accuracy() == 0) {
            battle_formula_calculate_mp_percent_damage();
        }
    }
}
