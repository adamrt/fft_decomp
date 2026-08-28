#include "fft/battle.h"
#include "psx/types.h"

void battle_formula_apply_quick_effect(void) {
    g_battle_action_target_data->ct_change = 0xFF;
    g_battle_action_target_data->attack_type = BATTLE_ACTION_TYPE_PSEUDO_STATUS;
}
