#include "fft/battle_ai.h"

s32 battle_ai_run_cowardly_movement(void) {
    battle_ai_data_t* ai = &g_battle_ai_data_base;
    s32 state;

    if (g_battle_ai_data_base.decision_state != 0) {
        /* Resume: re-enter the step that suspended. */
        state = g_battle_ai_cowardly_movement_step;
        if (state == 0) {
            goto step_a;
        }
        if (state == 1) {
            goto step_b;
        }
    }
    g_battle_ai_data_base.movement_scenario = 0;
step_a:
    if (battle_ai_score_reachable_tiles() == -1) {
        g_battle_ai_cowardly_movement_step = 0;
        return -1;
    }
step_b:
    if (battle_ai_fill_target_distance_grid(&ai->acting_unit_decision->target) == -1) {
        g_battle_ai_cowardly_movement_step = 1;
        return -1;
    }
    battle_ai_select_destination(0xff, BATTLE_AI_DESTINATION_PRIORITY_FIRST);
    ai->movement_scenario = 1;
    ai->candidate_coords[1].word = ai->candidate_coords[0].word;
    battle_ai_set_candidate_and_actor_reachable_tiles();
    return 0;
}
