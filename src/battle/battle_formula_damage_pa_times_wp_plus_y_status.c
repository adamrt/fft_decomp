#include "fft/battle.h"
#include "psx/types.h"

/* Formula 0x2D: damage (PA * (WP + Y)), 100% status. */
void battle_formula_damage_pa_times_wp_plus_y_status(void) {
    if (battle_formula_calculate_physical_evade() == 0) {
        battle_formula_store_pa_and_weapon_power_plus_y();
        battle_formula_apply_weapon_element_strengthen();
        battle_formula_calculate_physical_damage();
        battle_formula_modify_elemental_damage();
        if (g_battle_action_target_data->hit != 0) {
            battle_formula_apply_elemental_absorption();
            battle_formula_apply_status();
        }
    }
}
