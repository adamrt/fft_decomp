#include "fft/battle.h"

/*
 * Select an action for the recovery-oriented AI policy.
 *
 * Check urgent statuses before the designated-target, enemy and ally passes.
 * Return -1 to suspend and 0 when selection completes.
 */
s32 battle_ai_select_recovery_action(void) {
    battle_stats_t* acting = g_battle_ai_temp_unit_data;
    battle_ai_recovery_status_list_t statuses = g_battle_ai_recovery_status_ids;
    s32 target = acting->auto_battle_target;
    battle_ai_data_t* ai = &g_battle_ai_data_base;
    u32 status;
    s32 result;

    if (g_battle_ai_data_base.decision_state) {
        /* Resume: each phase re-enters the step that suspended. */
        switch (g_battle_ai_data_base.action_selection_phase) {
        case 0:
            goto score_tiles;
        case 1:
            goto movement;
        case 2:
            goto charging;
        case 3:
            goto target_abilities;
        case 4:
            goto enemy_abilities;
        case 7:
            goto cancel_status;
        default:
            goto ally_abilities;
        }
    }
    g_battle_ai_data_base.movement_scenario = 0;
score_tiles:
    if (battle_ai_score_reachable_tiles() == -1) {
        ai->action_selection_phase = 0;
        return -1;
    }
    g_battle_ai_recovery_status_retry = 0;
    if (((ai->unit_decisions[target].targeting_flags_2 >> 4) & (BATTLE_AI_TARGET_UNTARGETABLE >> 4))
        || ai->unit_decisions[target].enemy_flag) {
    retarget:
        battle_ai_select_peril_target();
        target = ai->acting_unit->auto_battle_target;
    }
    battle_ai_store_main_target_id_and_focus_on_target_flag(target);
movement:
    if (battle_ai_build_targeted_movement() == -1) {
        ai->action_selection_phase = 1;
        return -1;
    }
    ai->movement_scenario = 2;
    battle_ai_invert_target_priority();
    ai->movement_scenario = 0;
    if (!g_battle_ai_recovery_status_retry
        && !((ai->unit_decisions[target].targeting_flags_2 >> 6) & (BATTLE_AI_TARGET_DEAD_WITH_RERAISE >> 6))) {
        g_battle_ai_recovery_status_index = 0;
        do {
            u32 status_flags
                = g_battle_unit_stats[target]
                      .status_sets.current[(status = ((u8*)statuses.ids)[g_battle_ai_recovery_status_index]) >> 3];
            if (status_flags & (0x80 >> (status & 7))) {
            cancel_status:
                result = battle_ai_evaluate_status_cancellation(
                    target, ((u8*)statuses.ids)[g_battle_ai_recovery_status_index]);
                if (result == -1) {
                    ai->action_selection_phase = 7;
                    return -1;
                }
                if (result == 2)
                    return 0;
                if (result == 0) {
                    /* Retry target selection once, then skip further
                     * status-cancel attempts; the retry re-enters the
                     * retarget block above. */
                    g_battle_ai_recovery_status_retry = 1;
                    goto retarget;
                }
            }
            g_battle_ai_recovery_status_index++;
        } while (g_battle_ai_recovery_status_index < BATTLE_AI_RECOVERY_STATUS_COUNT);
    }
    ai->movement_scenario = 1;
charging:
    result = battle_ai_evaluate_charging_movement();
    switch (result) {
    case -1:
        ai->action_selection_phase = 2;
        return -1;
    case 1:
        return 0;
    }
    ai->movement_scenario = 0;
    if (battle_ai_select_active_units(BATTLE_AI_UNIT_FILTER_SPECIFIC, target)) {
    target_abilities:
        if (battle_ai_choose_move_from_move_list() == -1) {
            ai->action_selection_phase = 3;
            return -1;
        }
        if (battle_ai_select_ranked_action())
            return 0;
    }
    ai->movement_scenario = 1;
    if (battle_ai_select_active_units(BATTLE_AI_UNIT_FILTER_ENEMIES, 0)) {
    enemy_abilities:
        if (battle_ai_choose_move_from_move_list() == -1) {
            ai->action_selection_phase = 4;
            return -1;
        }
        if (battle_ai_select_ranked_action())
            return 0;
    }
    if (battle_ai_select_active_units(BATTLE_AI_UNIT_FILTER_ALLIES, 0)) {
    ally_abilities:
        if (battle_ai_choose_move_from_move_list() != -1) {
            if (battle_ai_select_ranked_action())
                return 0;
        } else {
            ai->action_selection_phase = 6;
            return -1;
        }
    }
    ai->movement_scenario = 2;
    battle_ai_transfer_ability_data_and_set_defend_flag();
    return 0;
}
