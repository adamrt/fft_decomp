#include "fft/battle.h"
#include "fft/data.h"
#include "psx/types.h"

void battle_action_check_counter_reaction_usability(u16 reaction_id, u32 mask) {
    battle_action_data_t* action;

    if (g_current_ability.skillset == SKILLSET_ID_MATH_SKILL)
        return;
    if ((g_current_ability.range_data.flags_4 & mask) == 0)
        return;
    if (battle_action_calculate_chance_to_react(g_battle_action_target) != 0)
        return;
    action = g_battle_action_target_data;
    action->reaction_id = reaction_id;
    action->last_received_attack = g_current_ability.ability_id;
}
