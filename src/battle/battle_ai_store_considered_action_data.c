#include "fft/battle_ai.h"

/*
 * Enter AI action simulation and preserve the current-ability scratch prefix.
 *
 * The saved span is exactly 30 bytes beginning at attacker facing; it is not a
 * complete battle-unit record. Hamedo state is cleared before simulation.
 */
void battle_ai_store_considered_action_data(void) {
    g_battle_action_state = BATTLE_ACTION_STATE_AI_SIMULATION;
    g_current_ability_hamedo_flag = 0;
    main_util_copy_byte_data(
        &g_current_ability_attacker, g_battle_ai_current_ability_data_backup, sizeof(g_current_ability_attacker));
}
