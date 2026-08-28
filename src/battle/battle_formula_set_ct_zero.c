#include "fft/battle.h"
#include "psx/types.h"

void battle_formula_set_ct_zero(void) {
    if (battle_formula_calculate_magical_evade() == 0) {
        if (battle_formula_calculate_magic_accuracy_no_elemental() == 0) {
            g_battle_action_target_data->ct_change = 0x7f;
            g_battle_action_target_data->attack_type = BATTLE_ACTION_TYPE_PSEUDO_STATUS;
        }
    }
}
