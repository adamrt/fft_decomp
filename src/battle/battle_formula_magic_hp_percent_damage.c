#include "fft/battle.h"
#include "psx/types.h"

void battle_formula_magic_hp_percent_damage(void) {
    if (battle_formula_calculate_magical_evade() == 0) {
        if (battle_formula_calculate_magic_accuracy() == 0) {
            battle_formula_calculate_hp_percent_damage();
            battle_formula_apply_weather_elemental_effects();
            battle_formula_apply_elemental();
            if (g_battle_action_target_data->hit != 0) {
                if (battle_formula_apply_elemental_absorption_and_status_proc() == 0) {
                    battle_formula_apply_status();
                }
            }
        }
    }
}
