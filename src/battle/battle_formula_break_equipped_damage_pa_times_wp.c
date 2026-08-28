#include "fft/battle.h"
#include "psx/types.h"

void battle_formula_break_equipped_damage_pa_times_wp(void) {
    battle_action_data_t* action;

    if (battle_formula_calculate_physical_evade() != 0) {
        return;
    }
    if (battle_formula_apply_steal_break_might_sword_hardcoding() != 0) {
        battle_formula_nullify_action();
        g_battle_action_target_data->miss_type = BATTLE_ACTION_MISS_TYPE_FORCED_FAILURE;
        return;
    }
    battle_formula_apply_maintenance();
    action = g_battle_action_target_data;
    if (action->hit == 0) {
        return;
    }
    /* The target writes the combined special-effect field as one halfword. */
    action->special_effect = BATTLE_ACTION_SPECIAL_EFFECT_BREAK_EQUIPMENT;
    battle_formula_store_pa_and_weapon_power();
    battle_formula_apply_weapon_element_strengthen();
    battle_formula_calculate_physical_damage();
    battle_formula_modify_elemental_damage();
    if (g_battle_action_target_data->hit == 0) {
        return;
    }
    battle_formula_apply_elemental_absorption();
}
