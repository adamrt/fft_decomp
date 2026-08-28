#include "fft/battle_ai.h"
#include "fft/unit_slots.h"

/*
 * Evaluate reachable origins for reflecting an ability onto another unit.
 *
 * Return -1 to suspend and 0 after restoring the temporary unit's position
 * and submitting the action to the ranked list. Stored counters survive
 * suspension, but the target does not reload the local reflector pointer
 * on resume; later target iterations can therefore use the caller's s2.
 */
s32 battle_ai_evaluate_reflected_target_origins(void) {
    battle_ai_data_t* ai = &g_battle_ai_data_base;
    battle_stats_t* reflector;
    battle_ai_unit_decision_t* decision;
    battle_ai_tile_mask_mode_e mode;
    s32 ct;

    if (g_battle_ai_data_base.decision_state) {
        /* Resume: the target jumps into the middle of the origin loop. */
        goto evaluate_targets;
    }
    g_battle_ai_data_base.current_action.reflected_action = 1;
    g_battle_ai_data_base.targetability.live.target_setting_flags
        &= ~BATTLE_AI_TARGET_SETTING_CONSUME_EVALUATED_ORIGINS;
    for (ai->reflector_unit_index = 0; ai->reflector_unit_index < BATTLE_UNIT_SLOT_COUNT; ai->reflector_unit_index++) {
        reflector = &g_battle_unit_stats[ai->reflector_unit_index];
        decision = &ai->unit_decisions[ai->reflector_unit_index];
        if (decision->targeting_flags_2 & BATTLE_AI_TARGET_UNTARGETABLE)
            continue;
        if (ai->reflector_unit_index == ai->acting_unit_id)
            continue;
        if (!battle_ai_is_status_active_through_delay(ai->considered_ability.ct, reflector, BATTLE_STATUS_ID_REFLECT))
            continue;
        if (battle_ai_check_ability_use_based_on_ct(ai->reflector_unit_index))
            continue;
        if (decision->targeting_flags_2 & BATTLE_AI_TARGET_DEAD_WITHOUT_RERAISE) {
            ct = ai->considered_ability.ct;
            if (battle_ai_calculate_clockticks_until_death_counter_expires(reflector) < ct)
                continue;
        }
        ai->current_action.targeting_type = 5;
        battle_ai_transfer_unit_coordinates(ai->reflector_unit_index, &ai->considered_unit_coords);
        mode = BATTLE_AI_TILE_MASK_AOE;
        if (ai->current_action.targeting_type == 6)
            mode = BATTLE_AI_TILE_MASK_EXPLICIT;
        battle_ai_build_targetable_tile_mask(mode, 0);
        battle_ai_transfer_halfword_values(ai->attack_origin_tiles, ai->targetable_tiles, 72);
        for (ai->reflected_target_unit_index = 0; ai->reflected_target_unit_index < BATTLE_UNIT_SLOT_COUNT;
            ai->reflected_target_unit_index++) {
            if (!ai->targetability.live.unit_targetable[ai->reflected_target_unit_index])
                continue;
            if (ai->reflected_target_unit_index == ai->reflector_unit_index)
                continue;
            if (ai->reflected_target_unit_index == ai->acting_unit_id)
                continue;
            if (battle_ai_check_ability_use_based_on_ct(ai->reflected_target_unit_index))
                continue;
            if (battle_ai_classify_ability_effect(ai->reflected_target_unit_index, 0)
                != BATTLE_AI_ABILITY_EFFECT_USEFUL)
                continue;
            ai->current_action.coords.bytes.x
                = reflector->x * 2 - g_battle_unit_stats[ai->reflected_target_unit_index].x;
            ai->current_action.coords.bytes.y
                = reflector->position.bits.y * 2 - g_battle_unit_stats[ai->reflected_target_unit_index].position.bits.y;
            if (ai->current_action.coords.bytes.x >= ai->map_max_x)
                continue;
            if (ai->current_action.coords.bytes.y >= ai->map_max_y)
                continue;
            for (ai->reflected_candidate_level = 0; ai->reflected_candidate_level < 2;
                ai->reflected_candidate_level++) {
                ai->current_action.coords.bytes.elevation = ai->reflected_candidate_level;
                if ((ai->reachable_tiles[ai->movement_scenario][ai->reflected_candidate_level]
                                        [ai->current_action.coords.bytes.y]
                        << ai->current_action.coords.bytes.x)
                    & 0x8000) {
                    battle_ai_check_unit_for_crystal_or_treasure_status();
                    ai->current_action.target_flags_set = 1;
                    ai->reflected_origin_phase = 0;
                    for (;;) {
                        if (battle_ai_check_ability_use_at_coords(&ai->acting_unit_coords)) {
                        evaluate_targets:
                            if (battle_ai_evaluate_target_tiles() == -1)
                                return -1;
                        }
                        if (ai->reflected_origin_phase != 0)
                            break;
                        ai->reflected_origin_phase = 1;
                        ai->current_action.target_flags_set = 0;
                    }
                }
            }
        }
    }
    battle_ai_move_temp_unit_to_coords(&ai->acting_unit_coords);
    ai->current_action.reflected_action = 0;
    battle_ai_insert_ranked_action();
    return 0;
}
