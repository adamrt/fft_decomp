#include "fft/battle.h"
#include "psx/types.h"

void battle_formula_steal_exp_hit_sp_x_percent(void) {
    if (battle_formula_calculate_physical_evade() == 0) {
        battle_formula_store_speed_and_x();
        battle_formula_apply_attack_up_and_martial_arts();
        battle_formula_apply_attacker_berserk_frog();
        battle_formula_apply_defense_up();
        battle_formula_apply_target_xa_affecting_statuses();
        battle_formula_apply_zodiac_compatibility();
        battle_formula_store_xa_plus_ya_status_damage();
        battle_formula_use_hp_damage_as_action_hit_percent();
        if (g_battle_action_target_data->hit != 0 || g_battle_action_state != BATTLE_ACTION_STATE_EXECUTE) {
            battle_formula_set_exp_stolen();
        }
    }
}
