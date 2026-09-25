#include "fft/battle.h"
#include "psx/types.h"

void battle_formula_heal_random_one_to_z_add_status(void) {
    battle_stats_t* unit;

    if (battle_formula_apply_status_and_check_undead() == 0)
        return;
    unit = g_battle_action_target;
    if ((unit->status_sets.current[0] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_UNDEAD)) != 0) {
        g_battle_action_target_data->hp_damage = unit->hp;
    } else {
        g_battle_action_target_data->hp_damage
            = (s32)(battle_formula_get_random_0_7fff() * g_main_item_secondary_data[g_current_ability.used_item_id].z)
                / 32768
            + 1;
    }
    battle_formula_apply_undead_reversal();
}
