#include "fft/battle.h"

/*
 * Initialize and dispatch the AI action-selection passes.
 *
 * Resume the selected pass without repeating action-record initialization.
 * Return -1 to suspend and 0 when that pass completes.
 */
s32 battle_ai_select_initial_action(void) {
    battle_ai_data_t* ai = &g_battle_ai_data_base;
    s32 result;

    if (g_battle_ai_data_base.decision_state) {
        /* Resume: re-enter the step that suspended. */
        switch (g_battle_ai_data_base.initial_targeting_state) {
        case 0:
            goto status_action;
        case 2:
            goto fallback_action;
        case 3:
            goto native_action;
        default:
            goto general_action;
        }
    }
    for (result = 0; result < 8; result++)
        battle_ai_reset_action_ranking_fields(&ai->ranked_actions[result]);
    battle_ai_reset_action_ranking_fields(&ai->current_action);
    battle_ai_reset_action_ranking_fields(&ai->best_action);
    battle_ai_reset_action_ranking_fields(&ai->inverted_priority_action);
    battle_ai_reset_action_ranking_fields(&ai->selected_action);
    battle_ai_fill_foe_proximity_grid();
status_action:
    result = battle_ai_choose_status_action();
    if (result == -1) {
        ai->initial_targeting_state = 0;
        return -1;
    }
    if (result != 0)
        return 0;
    if (((ai->acting_unit->status_sets.current[2]
             & (BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_FROG) | BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_CRITICAL)))
            && (ai->targetability.live.target_setting_flags & BATTLE_AI_TARGET_SETTING_RETREAT_SUPPORT_AVAILABLE))
        || ((ai->acting_unit->status_sets.current[4] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_DONT_ACT))
            && (ai->acting_unit_decision->flags & BATTLE_AI_DECISION_FOCUS_TARGET))) {
        battle_ai_transfer_halfword_values(
            (u16*)&ai->saved_acting_unit_decision, (u16*)ai->acting_unit_decision, sizeof(battle_ai_unit_decision_t));
    /* Resume after the save; restore the decision only when this pass ends. */
    fallback_action:
        if (battle_ai_select_retreat_action() == -1) {
            ai->initial_targeting_state = 2;
            return -1;
        }
        battle_ai_transfer_halfword_values(
            (u16*)ai->acting_unit_decision, (u16*)&ai->saved_acting_unit_decision, sizeof(battle_ai_unit_decision_t));
        return 0;
    }
native_action:
    result = battle_ai_handle_autobattle();
    if (result == -1) {
        ai->initial_targeting_state = 3;
        return -1;
    }
    if (result != 0)
        return 0;
    ai->decision_state = 0;
general_action:
    if (battle_ai_run_non_specific_autobattle() == -1) {
        ai->initial_targeting_state = 7;
        return -1;
    }
    return 0;
}
