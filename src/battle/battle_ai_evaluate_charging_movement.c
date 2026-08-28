#include "fft/battle_ai.h"
#include "fft/main_runtime.h"

/* Evaluate movement while keeping or removing the acting unit's charge.
 *
 * The second simulation temporarily changes unit data; a suspended pass
 * preserves those changes until resumed and completed. The simulation helper
 * resumes from its saved phase when the frame budget is exhausted.
 */
s32 battle_ai_evaluate_charging_movement(void) {
    battle_stats_t* unit = g_battle_ai_temp_unit_data;
    battle_ai_data_t* ai = &g_battle_ai_data_base;
    battle_action_command_prefix_t* action = (battle_action_command_prefix_t*)&unit->action_actor_id;
    s32 state;
    s32 suspended_stage;

    if (g_battle_ai_data_base.decision_state) {
        /* Resume: re-enter the step that suspended. */
        state = g_battle_ai_data_base.charging_state;
        if (state == 0)
            goto evaluate_song_dance;
        if (state == 1)
            goto evaluate_with_charge;
        goto evaluate_without_charge;
    }
    if (unit->charged_ability_ct == 0xff)
        return 0;
    if (g_main_action_menu_types_by_skillset[unit->last_skillset_id] == ACTION_MENU_TYPE_CHARGE) {
        g_battle_ai_data_base.current_action.coords.word = g_battle_ai_data_base.acting_unit_coords.word;
    } else {
        g_battle_ai_data_base.current_action.coords.word
            = ai->candidate_coords[g_battle_ai_data_base.movement_scenario].word;
        battle_ai_check_unit_for_crystal_or_treasure_status();
    }
    ai->current_action.target_flags_set = 0;
    ai->current_action.skillset = 0;
    if ((u32)(action->ability_id - 0x56) < 14) {
    evaluate_song_dance:
        if (battle_ai_check_set_highest_unit_priority() == -1) {
            ai->charging_state = 0;
            return -1;
        }
        battle_ai_insert_ranked_action();
        return 0;
    }
evaluate_with_charge:
    if (battle_ai_simulate_action_and_score() == -1) {
        suspended_stage = 1;
        goto suspend; /* shared suspend tail; a direct store changes the layout */
    }
    battle_ai_transfer_halfword_values(
        (u16*)&ai->selected_action, (u16*)&ai->current_action, sizeof(battle_ai_action_data_t));
    ai->saved_ability_ct = ai->acting_unit->charged_ability_ct;
    ai->saved_current_status = ai->acting_unit->status_sets.current[0];
    ai->saved_inflicted_status = ai->acting_unit->inflicted_status[0];
    ai->acting_unit->charged_ability_ct = 0xff;
    ai->acting_unit->status_sets.current[0]
        &= ~(BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_CHARGING) | BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_PERFORMING));
    ai->acting_unit->inflicted_status[0]
        &= ~(BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_CHARGING) | BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_PERFORMING));
evaluate_without_charge:
    if (battle_ai_simulate_action_and_score() == -1) {
        suspended_stage = 2;
    suspend:
        ai->charging_state = suspended_stage;
        return -1;
    }
    ai->acting_unit->charged_ability_ct = ai->saved_ability_ct;
    ai->acting_unit->status_sets.current[0] = ai->saved_current_status;
    ai->acting_unit->inflicted_status[0] = ai->saved_inflicted_status;
    return battle_ai_compare_target_priority_and_hit_rate((battle_ai_action_rank_t*)&ai->selected_action.rank_byte,
               (battle_ai_action_rank_t*)&ai->current_action.rank_byte)
        == BATTLE_AI_RANK_ABOVE;
}
