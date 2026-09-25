#include "fft/battle.h"

/*
 * Run or resume the general autobattle action search.
 *
 * Return -1 to suspend and 0 when the selected action is ready. Keeping a
 * charged action skips the new ability search. The switch preserves the
 * target's completion-return delay slot.
 */
s32 battle_ai_run_non_specific_autobattle(void) {
    battle_ai_data_t* ai = &g_battle_ai_data_base;
    s32 charging_result;
    s32 resume_state;

    if (g_battle_ai_data_base.decision_state != 0) {
        /* Resume: re-enter the step that suspended. */
        resume_state = g_battle_ai_data_base.autobattle_state;
        if (resume_state == 4)
            goto evaluate_movement;
        if (resume_state == 2)
            goto evaluate_charging;
        goto evaluate_abilities;
    }
    g_battle_ai_data_base.movement_scenario = 1;
evaluate_movement:
    if (battle_ai_build_movement_scenarios() == -1) {
        ai->autobattle_state = 4;
        return -1;
    }
    ai->movement_scenario = 0;
    battle_ai_invert_target_priority();
    ai->movement_scenario = 1;
evaluate_charging:
    charging_result = battle_ai_evaluate_charging_movement();
    switch (charging_result) {
    case -1:
        ai->autobattle_state = 2;
        return -1;
    case 1:
        return 0;
    }
    ai->movement_scenario = 1;
    if (battle_ai_select_active_units(BATTLE_AI_UNIT_FILTER_ANY, 0) != 0) {
    evaluate_abilities:
        if (battle_ai_choose_move_from_move_list() == -1) {
            ai->autobattle_state = 7;
            return -1;
        }
    }
    if (battle_ai_select_ranked_action() == 0) {
        ai->movement_scenario = 0;
        battle_ai_transfer_ability_data_and_set_defend_flag();
    }
    return 0;
}
