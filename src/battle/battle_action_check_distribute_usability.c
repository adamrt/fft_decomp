#include "fft/battle.h"
#include "fft/data.h"
#include "psx/types.h"

void battle_action_check_distribute_usability(void) {
    battle_stats_t* target = g_battle_action_target;
    battle_action_data_t* action = g_battle_action_target_data;
    /* hp_healing is read signed (lh) here. */
    s32 excess = *(s16*)&action->hp_healing - (s32)(target->max_hp - target->hp);
    /* The target passes the unit twice to the one-parameter callee. */
    if (excess > 0
        && ((s32 (*)(const battle_stats_t*, const battle_stats_t*))battle_action_calculate_chance_to_react)(
               target, target)
            == 0) {
        g_battle_action_target_data->last_received_attack = (s16)excess;
        g_battle_action_target_data->reaction_id = ABILITY_ID_REACTION_DISTRIBUTE;
    }
}
