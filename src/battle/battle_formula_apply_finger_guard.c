#include "fft/battle.h"
#include "psx/types.h"

/* Cancel the action with Finger Guard when the target's Brave-based reaction roll succeeds. */
void battle_formula_apply_finger_guard(void) {
    battle_stats_t* unit;
    s32 chance;

    if (battle_action_can_unit_react_1(g_battle_action_target) != 0)
        return;
    unit = g_battle_action_target;
    if ((unit->reaction_abilities[3] & BATTLE_REACTION_SET_4_FINGER_GUARD) == 0)
        return;
    chance = 0x64 - unit->brave;
    /* The target stores the accuracy as a halfword (0x2a..0x2b). */
    g_battle_action_target_data->attack_accuracy = (s16)chance;
    if (chance != 0) {
        if (battle_action_calculate_chance_to_react(unit) != 0)
            return;
        if (g_battle_action_state != BATTLE_ACTION_STATE_EXECUTE)
            return;
    }
    g_battle_action_target_data->hit = 0;
    g_battle_action_target_data->reaction_id = ABILITY_ID_REACTION_FINGER_GUARD;
    g_battle_action_target_data->miss_type = BATTLE_ACTION_MISS_TYPE_BLADE_GRASP_OR_FINGER_GUARD;
}
