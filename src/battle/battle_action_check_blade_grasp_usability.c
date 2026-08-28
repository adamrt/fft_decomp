#include "fft/battle.h"
#include "fft/data.h"
#include "psx/types.h"

void battle_action_check_blade_grasp_usability(void) {
    battle_stats_t* attacker;
    battle_action_data_t* target;

    if ((g_current_ability.range_data.flags_4 & ABILITY_SECONDARY_FLAG_4_BLADE_GRASP_ELIGIBLE) == 0)
        return;
    attacker = g_battle_action_target;
    target = g_battle_action_target_data;
    /* Halfword store covers attack_accuracy and the pad byte after it. */
    target->attack_accuracy = (s16)(0x64 - attacker->brave);
    if (battle_action_calculate_chance_to_react(attacker) != 0)
        return;
    if (g_battle_action_state != BATTLE_ACTION_STATE_EXECUTE)
        return;
    g_battle_action_target_data->hit = 0;
    g_battle_action_target_data->miss_type = BATTLE_ACTION_MISS_TYPE_BLADE_GRASP_OR_FINGER_GUARD;
    g_battle_action_target_data->reaction_id = ABILITY_ID_REACTION_BLADE_GRASP;
}
