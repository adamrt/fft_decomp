#include "fft/battle.h"

s32 battle_ai_check_set_highest_unit_priority(void) {
    battle_ai_data_t* ai = &g_battle_ai_data_base;
    s32 result = battle_ai_simulate_action_and_score();

    if (result == -1) {
        return -1;
    }
    if (result != 0) {
        if (battle_ai_is_action_higher_ranked(&ai->current_action, &ai->best_action) != 0) {
            battle_ai_transfer_halfword_values(
                (u16*)&ai->best_action, (u16*)&ai->current_action, sizeof(battle_ai_action_data_t));
        }
    }
    return 0;
}
