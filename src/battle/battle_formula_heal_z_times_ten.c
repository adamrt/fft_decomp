#include "fft/battle.h"
#include "psx/types.h"

/* Formula 0x48: heal (Z * 10) HP. */
void battle_formula_heal_z_times_ten(void) {
    g_battle_action_target_data->hp_damage = g_main_item_secondary_data[g_current_ability.used_item_id].z * 10;
    battle_formula_apply_undead_reversal();
}
