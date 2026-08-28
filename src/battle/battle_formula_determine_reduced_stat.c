#include "fft/battle.h"
#include "fft/data.h"
#include "psx/types.h"

void battle_formula_determine_reduced_stat(void) {
    u16 ability;

    ability = g_current_ability.ability_id;
    if (ability == ABILITY_ID_BATTLE_SKILL_POWER_BREAK || ability == ABILITY_ID_MONSTER_SKILL_POWER_RUIN) {
        g_battle_action_target_data->pa_change = g_current_ability.range_data.x & BATTLE_ACTION_STAT_CHANGE_VALUE_MASK;
    } else if (ability == ABILITY_ID_BATTLE_SKILL_MIND_BREAK || ability == ABILITY_ID_MONSTER_SKILL_MIND_RUIN) {
        g_battle_action_target_data->ma_change = g_current_ability.range_data.x & BATTLE_ACTION_STAT_CHANGE_VALUE_MASK;
    } else if (ability == ABILITY_ID_BATTLE_SKILL_SPEED_BREAK || ability == ABILITY_ID_MONSTER_SKILL_SPEED_RUIN) {
        g_battle_action_target_data->sp_change = g_current_ability.range_data.x & BATTLE_ACTION_STAT_CHANGE_VALUE_MASK;
    } else {
        return;
    }
    g_battle_action_target_data->attack_type |= BATTLE_ACTION_TYPE_PSEUDO_STATUS;
}
