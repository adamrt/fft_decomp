#include "fft/battle.h"

void battle_formula_weapon_damage_with_proc(void) {
    if (battle_formula_calculate_physical_evade() == 0) {
        if (battle_formula_calculate_weapon_damage() == 0) {
            battle_formula_store_reaction_proc_id_and_target();
        }
    }
}
