#include "fft/battle.h"

/*
 * Select an AI action around the unit's designated target.
 *
 * Preserve the target across the staged movement and ability passes. Return
 * -1 to suspend and 0 when selection completes.
 */
s32 battle_ai_select_targeted_action(void) {
    s32 target = g_battle_ai_temp_unit_data->auto_battle_target;
    battle_ai_data_t* ai = &g_battle_ai_data_base;
    battle_stats_t* unit;
    s32 i, result;

    /* Reload the persistent target before every resume dispatch. */
    if (g_battle_ai_data_base.decision_state) {
        switch (g_battle_ai_data_base.action_selection_phase) {
        case 0:
            goto score_tiles;
        case 1:
            goto movement;
        case 2:
            goto charging;
        case 3:
            goto target_abilities;
        default:
            goto other_abilities;
        }
    }
    if (battle_ai_check_target_type(target) || !ai->unit_decisions[target].enemy_flag) {
        g_battle_ai_data_base.acting_unit_decision->flags &= ~BATTLE_AI_DECISION_SPECIAL_BEHAVIOR;
        unit = &g_battle_unit_stats[target];
        if (unit->mount_info & BATTLE_MOUNT_INFO_FLAG_MOUNT) {
            if (!((unit->status_sets.current[2] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_TRANSPARENT))
                    && (g_battle_unit_stats[unit->mount_info & BATTLE_MOUNT_INFO_PARTNER_ID_MASK].status_sets.current[2]
                        & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_TRANSPARENT))))
                target = unit->mount_info & BATTLE_MOUNT_INFO_PARTNER_ID_MASK;
        } else {
            /* A direct member pointer folds the target's base cursor and field
             * displacement together. Derive the member offset to retain its stores. */
            u32 cursor;
            s32 included = 1;
            i = BATTLE_UNIT_SLOT_COUNT - 1;
            cursor = (u32)ai + i;
            do {
                *(u8*)(cursor + ((u32)&ai->targetability.live.unit_targetable - (u32)ai)) = included;
                i--;
                cursor--;
            } while (i >= 0);
            target = battle_ai_find_nearest_target(BATTLE_AI_NEAREST_ELIGIBLE_ENEMY);
            if (target == ai->acting_unit_id) {
                for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
                    unit = &g_battle_unit_stats[i];
                    if (unit->entd_slot != BATTLE_ENTD_SLOT_NONE && ai->unit_decisions[i].enemy_flag
                        && (unit->status_sets.current[2] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_TRANSPARENT))) {
                        target = i;
                        break;
                    }
                }
            }
        }
    }
    ai->acting_unit->auto_battle_target = target;
    battle_ai_store_main_target_id_and_focus_on_target_flag(target);
    ai->movement_scenario = 0;
score_tiles:
    if (battle_ai_score_reachable_tiles() == -1) {
        ai->action_selection_phase = 0;
        return -1;
    }
movement:
    if (battle_ai_build_targeted_movement() == -1) {
        ai->action_selection_phase = 1;
        return -1;
    }
    ai->movement_scenario = 2;
    battle_ai_invert_target_priority();
    ai->movement_scenario = 0;
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
    if (battle_ai_select_active_units(BATTLE_AI_UNIT_FILTER_ANY, 0)) {
    other_abilities:
        if (battle_ai_choose_move_from_move_list() == -1) {
            ai->action_selection_phase = 4;
            return -1;
        }
        if (battle_ai_select_ranked_action())
            return 0;
    }
    ai->movement_scenario = 2;
    battle_ai_transfer_ability_data_and_set_defend_flag();
    return 0;
}
