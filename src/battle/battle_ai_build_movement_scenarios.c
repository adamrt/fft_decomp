#include "fft/battle.h"

/* Workspace aliases preserve the target's separate entry address loads. */
extern s32 g_battle_ai_target_in_range;             /* 1 for range >= distance or charging; 0 otherwise. */
extern s32 g_battle_ai_closest_ability_range;       /* Movement range, possibly halved before tile selection. */
extern s32 g_battle_ai_movement_unit_index;         /* Reused unit/row loop index or Stay Near occupant ID. */
extern s32 g_battle_ai_target_distance;             /* Target-distance byte at the acting unit's tile. */
extern s32 g_battle_ai_stay_near_destination_fixed; /* 1 after a reachable Stay Near destination is fixed. */

/* Prepare candidate destinations and reachable movement scenarios.
 *
 * Return -1 when a child suspends and 0 after all scenarios are prepared.
 * Scratch words preserve branch decisions across suspension. Their broader
 * ownership and the address-named child helpers remain provisional.
 */
s32 battle_ai_build_movement_scenarios(void) {
    battle_ai_unit_decision_t* decision;
    battle_ai_data_t* ai;
    battle_stats_t* unit;
    s32 limit;
    s32 low;
    s32 high;

    g_battle_ai_movement_scenario = 1;
    decision = g_battle_ai_acting_unit_decision_ptr;
    ai = &g_battle_ai_data_base;
    if (g_battle_ai_data_base.decision_state) {
        /* Resume: re-enter the step that suspended. */
        if (g_battle_ai_data_base.movement_decision_state == 0)
            goto fill_target_distance;
        goto score_tiles;
    }
    if ((decision->flags & BATTLE_AI_DECISION_FOCUS_TARGET)
        && (decision->main_target_id == g_battle_ai_data_base.acting_unit_id
            || ((ai->unit_decisions[decision->main_target_id].targeting_flags_2 >> 4)
                & (BATTLE_AI_TARGET_UNTARGETABLE >> 4)))) {
        battle_ai_clear_words((s32*)ai->tile_target_distance, sizeof(ai->tile_target_distance));
    } else {
    fill_target_distance:
        if (battle_ai_fill_target_distance_grid(&decision->target) == -1) {
            ai->movement_decision_state = 0;
            return -1;
        }
        if ((decision->targeting_flags_2 & (BATTLE_AI_TARGET_HP_BELOW_HALF | BATTLE_AI_TARGET_MP_CONSTRAINED))
            && !(ai->acting_unit->status_sets.current[4] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_DEATH_SENTENCE))) {
            for (g_battle_ai_movement_unit_index = 0; g_battle_ai_movement_unit_index < BATTLE_UNIT_SLOT_COUNT;
                g_battle_ai_movement_unit_index++) {
                unit = &g_battle_unit_stats[g_battle_ai_movement_unit_index];
                if (unit->entd_slot != BATTLE_ENTD_SLOT_NONE
                    && (unit->status_sets.current[0] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_CRYSTAL))) {
                    battle_ai_transfer_unit_coordinates(g_battle_ai_movement_unit_index, &ai->considered_unit_coords);
                    ai->tile_target_distance[ai->considered_unit_coords.bytes.elevation]
                                            [ai->considered_unit_coords.bytes.y][ai->considered_unit_coords.bytes.x]
                        = 1;
                }
            }
        }
    }
    g_battle_ai_target_distance
        = ai->tile_target_distance[ai->acting_unit_coords.bytes.elevation][ai->acting_unit_coords.bytes.y]
                                  [ai->acting_unit_coords.bytes.x];
    g_battle_ai_closest_ability_range = battle_ai_get_movement_range();
    limit = g_battle_ai_target_distance;
    g_battle_ai_stay_near_destination_fixed = 0;
    if (g_battle_ai_closest_ability_range >= limit
        || (decision->targeting_flags_1 & BATTLE_AI_TARGETING_FLAG_1_REFLECTED_ACTION)) {
        g_battle_ai_target_in_range = 1;
        if (decision->flags & BATTLE_AI_DECISION_STAY_NEAR_COORDINATES) {
            g_battle_ai_movement_unit_index = battle_ai_find_unit_at_coordinates(&decision->target);
            if (g_battle_ai_movement_unit_index == ai->acting_unit_id
                || g_battle_ai_movement_unit_index == BATTLE_UNIT_ID_NONE) {
                if (ai->reachable_tiles[0][decision->target.bytes.elevation][decision->target.bytes.y]
                    & (0x8000 >> decision->target.bytes.x)) {
                    ai->candidate_coords[ai->movement_scenario] = decision->target;
                    battle_ai_set_candidate_and_actor_reachable_tiles();
                    g_battle_ai_stay_near_destination_fixed = 1;
                    goto score_tiles;
                }
            }
        }
        battle_ai_filter_reachable_tiles_by_target_distance(ai->acting_unit_move + g_battle_ai_target_distance);
    } else {
        g_battle_ai_target_in_range = 0;
        battle_ai_filter_reachable_tiles_by_target_distance(limit - 1);
    }
score_tiles:
    if (battle_ai_score_reachable_tiles() == -1) {
        ai->movement_decision_state = 1;
        return -1;
    }
    if (!g_battle_ai_stay_near_destination_fixed) {
        if (g_battle_ai_target_in_range) {
            battle_ai_select_destination(0x7fffffff, BATTLE_AI_DESTINATION_PRIORITY_FIRST);
        } else {
            if (ai->acting_unit_team && rand() % 7 == 0)
                g_battle_ai_closest_ability_range /= 2;
            battle_ai_select_destination_with_nearest_fallback(g_battle_ai_closest_ability_range);
        }
    }
    /* XOR preserves the target operation without assuming scenario 1 is a subset. */
    ai->movement_scenario = 2;
    for (g_battle_ai_movement_unit_index = 0; g_battle_ai_movement_unit_index < ai->map_max_y;
        g_battle_ai_movement_unit_index++) {
        low = ai->reachable_tiles[0][0][g_battle_ai_movement_unit_index]
            ^ ai->reachable_tiles[1][0][g_battle_ai_movement_unit_index];
        high = ai->reachable_tiles[0][1][g_battle_ai_movement_unit_index]
            ^ ai->reachable_tiles[1][1][g_battle_ai_movement_unit_index];
        ai->reachable_tiles[2][0][g_battle_ai_movement_unit_index] = low;
        ai->reachable_tiles[2][1][g_battle_ai_movement_unit_index] = high;
    }
    ai->candidate_coords[2].word = ai->candidate_coords[1].word;
    ai->movement_scenario = 0;
    if (!ai->acting_unit->action_taken && (decision->flags & BATTLE_AI_DECISION_FOCUS_TARGET)
        && g_battle_ai_target_in_range
        && !(decision->targeting_flags_1 & BATTLE_AI_TARGETING_FLAG_1_REFLECTED_ACTION)) {
        battle_ai_select_destination_with_range_floor(g_battle_ai_target_distance - 1);
    } else {
        ai->candidate_coords[0].word = ai->candidate_coords[1].word;
    }
    ai->reachable_tiles[1][ai->acting_unit_coords.bytes.elevation][ai->acting_unit_coords.bytes.y]
        |= 0x8000 >> ai->acting_unit_coords.bytes.x;
    return 0;
}
