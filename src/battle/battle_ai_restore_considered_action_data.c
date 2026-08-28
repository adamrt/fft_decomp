#include "fft/battle_ai.h"

void battle_ai_restore_considered_action_data(void) {
    g_battle_action_state = BATTLE_ACTION_STATE_EXECUTE;
    main_util_copy_byte_data(
        g_battle_ai_current_ability_data_backup, &g_current_ability_attacker, sizeof(g_current_ability_attacker));
}
