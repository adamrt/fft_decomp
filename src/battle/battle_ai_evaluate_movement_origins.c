#include "fft/battle.h"

/*
 * Evaluate target tiles from eligible movement origins.
 *
 * Return -1 with the search stage saved when time or a child suspends;
 * return 0 after restoring the temporary unit's position. The scan path
 * reloads its full row mask on resume and continues at its saved x index.
 */
s32 battle_ai_evaluate_movement_origins(void) {
    battle_ai_data_t* ai = &g_battle_ai_data_base;
    s32 row;
    u32 row_base;

    if (g_battle_ai_data_base.decision_state) {
        /* Resume: re-enter the step that suspended. */
        if (g_battle_ai_data_base.highest_priority_state == 0)
            goto begin_evaluation;
        if (g_battle_ai_data_base.highest_priority_state == 1)
            goto evaluate_ranked_origin;
        /* Typed indexing combines the level and row scaling; these member-
         * derived stages preserve the target's resume-load order. */
        row_base = g_battle_ai_data_base.map_level_counter * sizeof(u16[18]) + (u32)ai;
        row_base += g_battle_ai_data_base.y_counter * sizeof(u16);
        row = *(u16*)(row_base + ((u32)&ai->movable_tiles - (u32)ai));
        goto evaluate_scanned_origin;
    }
    row = ai->targetability.live.unit_targetable[g_battle_ai_data_base.acting_unit_id];
    ai->targetability.live.unit_targetable[g_battle_ai_data_base.acting_unit_id] = 0;
    battle_ai_evaluate_linear_ability_behaviors();
    ai->targetability.live.unit_targetable[g_battle_ai_data_base.acting_unit_id] = row;
begin_evaluation:
    if (VSync(1) >= 441) {
        ai->highest_priority_state = 0;
        return -1;
    }
    ai->decision_state = 0;
    ai->weapon_range_flag = 0;
    if (ai->considered_ability.ai_flags.word & BATTLE_AI_ABILITY_EXHAUSTIVE_ORIGIN_SCAN) {
        ai->targetability.live.target_setting_flags &= ~BATTLE_AI_TARGET_SETTING_CONSUME_EVALUATED_ORIGINS;
    } else {
        ai->max_possibilities = ai->attack_origin_tile_count;
        ai->max_possibilities *= ai->movable_tile_count;
        if (ai->max_possibilities > 10)
            ai->weapon_range_flag = 1;
        /* The target sets this regardless of the truncated product test. */
        ai->weapon_range_flag = 1;
    }
    if ((ai->targetability.live.target_setting_flags & BATTLE_AI_TARGET_SETTING_CONSUME_EVALUATED_ORIGINS)
        || ai->weapon_range_flag) {
        while (ai->attack_origin_tile_count && ai->movable_tile_count) {
            battle_ai_take_next_movement_origin();
            battle_ai_select_candidate_coords_and_check_crystal();
        evaluate_ranked_origin:
            row = battle_ai_evaluate_target_tiles();
            if (row == -1) {
                ai->highest_priority_state = 1;
                return -1;
            }
        }
    } else {
        ai->map_level_counter = 0;
        do {
            for (ai->y_counter = 0; ai->y_counter < ai->map_max_y; ai->y_counter++) {
                /* Preserve the same level/row address stages during scanning. */
                row_base = ai->map_level_counter * sizeof(u16[18]) + (u32)ai;
                row_base += ai->y_counter * sizeof(u16);
                row = *(u16*)(row_base + ((u32)&ai->movable_tiles - (u32)ai));
                if (row != 0) {
                    for (ai->max_possibilities = 0; ai->max_possibilities < ai->map_max_x; ai->max_possibilities++) {
                        if ((row << ai->max_possibilities) & 0x8000) {
                            ai->current_action.coords.bytes.x = ai->max_possibilities;
                            ai->current_action.coords.bytes.y = ai->y_counter;
                            ai->current_action.coords.bytes.elevation = ai->map_level_counter;
                            battle_ai_select_candidate_coords_and_check_crystal();
                        evaluate_scanned_origin:
                            if (battle_ai_evaluate_target_tiles() == -1) {
                                ai->highest_priority_state = 2;
                                return -1;
                            }
                        }
                    }
                }
            }
            ai->map_level_counter++;
        } while (ai->map_level_counter < 2);
    }
    battle_ai_move_temp_unit_to_coords(&ai->acting_unit_coords);
    return 0;
}
