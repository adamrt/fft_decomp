#include "fft/battle.h"
#include "psx/types.h"

/* Apply the target's reflect or weapon-evasion reactions before the attack resolves. */
void battle_action_check_reflect_blade_grasp_and_arrow_guard(void) {
    u8 reaction_flags;

    if (g_battle_action_context != BATTLE_ACTION_CONTEXT_PRIMARY) {
        return;
    }
    if (g_current_ability.formula == 7) {
        return;
    }
    if (battle_action_check_reaction(g_battle_action_target) == 1) {
        return;
    }
    if ((g_battle_action_target->status_sets.current[4] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_REFLECT))
        && g_current_ability.reaction_id == 0) {
        battle_action_check_reflect_reaction();
    }
    /* reaction_id is loaded signed (lh) here. */
    if (*(s16*)&g_battle_action_target_data->reaction_id != 0) {
        return;
    }
    if (battle_action_can_unit_react(g_battle_action_target) != 0) {
        return;
    }
    if (battle_formula_can_unit_evade(g_battle_action_target) != 0) {
        return;
    }
    reaction_flags = g_battle_action_target->reaction_abilities[3];
    if (reaction_flags & BATTLE_REACTION_SET_4_BLADE_GRASP) {
        battle_action_check_blade_grasp_usability();
        return;
    }
    if (reaction_flags & BATTLE_REACTION_SET_4_ARROW_GUARD) {
        battle_action_check_arrow_guard_usability();
    }
}
