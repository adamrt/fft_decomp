#include "fft/battle.h"
#include "psx/types.h"

/* Formula 0x40: undead targets only, hit (SP+X)%. */
void battle_formula_undead_hit_sp_x_percent(void) {
    battle_formula_store_speed_and_x();
    battle_formula_apply_physical_xa_modifying_statuses_support();
    battle_formula_store_xa_plus_ya_status_damage();
    battle_formula_use_hp_damage_as_action_hit_percent();
    if (g_battle_action_target_data->hit != 0) {
        if ((g_battle_action_target->status_sets.current[0] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_UNDEAD)) == 0) {
            battle_formula_force_attack_miss();
        } else {
            battle_formula_apply_status_to_action();
        }
    }
}
