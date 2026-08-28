#include "fft/battle.h"
#include "psx/types.h"

void battle_formula_break_equipped_hit_pa_wp_x_percent(void) {
    battle_action_data_t* action;

    if (battle_formula_apply_steal_break_might_sword_hardcoding() != 0) {
        g_current_ability.defaulted_to_attack = 1;
        battle_action_switch_ability_to_default_attack();
        return;
    }
    g_current_ability.defaulted_to_attack = 0;
    if (battle_formula_calculate_physical_evade() != 0) {
        return;
    }
    battle_formula_store_pa_and_weapon_power_plus_y();
    if (battle_formula_calculate_physical_accuracy() != 0) {
        /* The target writes the combined special-effect field as one halfword. */
        g_battle_action_target_data->special_effect = 0;
        return;
    }
    battle_formula_apply_maintenance();
    action = g_battle_action_target_data;
    if (action->hit != 0) {
        action->special_effect = BATTLE_ACTION_SPECIAL_EFFECT_BREAK_EQUIPMENT;
    }
}
