#include "fft/battle.h"
#include "psx/types.h"

void battle_formula_status_reduction(void) {
    if (battle_formula_calculate_magical_evade() == 0) {
        battle_formula_store_ma_and_y();
        battle_formula_apply_elemental_strengthen();
        battle_formula_apply_magical_support_status_compatibility();
        battle_formula_store_xa_plus_ya_status_damage();
        battle_formula_calculate_faith();
        battle_formula_use_hp_damage_as_action_hit_percent();
        if (g_battle_action_target_data->hit == 0) {
            /* The target clears the bit in the special_effect halfword. */
            g_battle_action_target_data->special_effect &= ~BATTLE_ACTION_SPECIAL_EFFECT_BREAK_EQUIPMENT;
        } else {
            battle_formula_determine_reduced_stat();
        }
    }
}
