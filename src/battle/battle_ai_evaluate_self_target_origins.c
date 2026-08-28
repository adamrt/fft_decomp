#include "fft/battle_ai.h"
#include "fft/data.h"
#include "fft/main_runtime.h"

/*
 * Evaluate self-target alternatives from the actor and candidate coordinates.
 *
 * Record the resume stage when returning -1 to suspend. Completed searches
 * restore the actor coordinates and submit the action to the ranked list.
 */
s32 battle_ai_evaluate_self_target_origins(void) {
    battle_ai_data_t* ai = &g_battle_ai_data_base;
    battle_ai_tile_mask_mode_e mode;

    if (g_battle_ai_data_base.decision_state) {
        /* Resume: re-enter the step that suspended. */
        if (g_battle_ai_self_target_origin_stage == 0)
            goto evaluate_map;
        goto evaluate_targets;
    }
    g_battle_ai_data_base.targetability.live.target_setting_flags
        &= ~BATTLE_AI_TARGET_SETTING_CONSUME_EVALUATED_ORIGINS;
    g_battle_ai_data_base.current_action.coords.word
        = ai->candidate_coords[g_battle_ai_data_base.movement_scenario].word;
    battle_ai_check_unit_for_crystal_or_treasure_status();
    if ((g_battle_ai_data_base.considered_ability.ai_flags.word & BATTLE_AI_ABILITY_FOLLOW_TARGET)
        && !(g_main_ability_range_data[(s16)g_battle_ai_data_base.considered_ability.ability_id].flags_1
            & ABILITY_SECONDARY_FLAG_1_CANNOT_TARGET_SELF)) {
        g_battle_ai_data_base.current_action.target_flags_set = 1;
        g_battle_ai_data_base.current_action.targeting_type = 6;
        g_battle_ai_data_base.current_action.target_id = g_battle_ai_data_base.acting_unit_id;
    evaluate_map:
        if (battle_ai_check_map_allows_use_and_find_highest_target() == -1) {
            g_battle_ai_self_target_origin_stage = 0;
            return -1;
        }
    }
    ai->current_action.targeting_type = 5;
    g_battle_ai_self_target_origin_stage = 0;
    ai->current_action.target_flags_set = 1;
    /* The first-member word view keeps this load after the phase store.
     * Direct .word or whole-union assignment changes target scheduling. */
    ai->considered_unit_coords.word = *(u32*)&ai->acting_unit_coords;
prepare_origin:
    mode = BATTLE_AI_TILE_MASK_AOE;
    if (ai->current_action.targeting_type == 6)
        mode = BATTLE_AI_TILE_MASK_EXPLICIT;
    battle_ai_build_targetable_tile_mask(mode, 0);
    battle_ai_transfer_halfword_values(ai->attack_origin_tiles, ai->targetable_tiles, 72);
    if (battle_ai_check_ability_use_at_coords(&ai->acting_unit_coords)) {
    evaluate_targets:
        if (battle_ai_evaluate_target_tiles() == -1) {
            g_battle_ai_self_target_origin_stage = 1;
            return -1;
        }
    }
    if (g_battle_ai_self_target_origin_stage == 0) {
        ai->current_action.target_flags_set = 0;
        ai->considered_unit_coords.word = ai->current_action.coords.word;
        g_battle_ai_self_target_origin_stage++;
        /* A goto retry, not a loop: GCC's loop pass would move the suspend
         * exit out of line. */
        goto prepare_origin;
    }
    battle_ai_move_temp_unit_to_coords(&ai->acting_unit_coords);
    battle_ai_insert_ranked_action();
    return 0;
}
