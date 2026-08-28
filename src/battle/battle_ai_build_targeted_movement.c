#include "fft/battle_ai.h"

/*
 * Prepare movement scenarios around the policy's designated target.
 *
 * Return -1 while target-distance propagation is suspended, otherwise 0.
 * Recovery mode limits its initial destination search to 2 when farther away;
 * scenario 2 may approach one tile closer if the actor has not acted.
 */
s32 battle_ai_build_targeted_movement(void) {
    battle_ai_data_t* ai = &g_battle_ai_data_base;
    s32 target;
    s32 range;
    u8(*distance_rows)[16];
    u32 distance_level_base;
    s32 limit;
    s32 distance;
    s32 within_two_tiles;

    if (!g_battle_ai_data_base.decision_state) {
        target = g_battle_ai_temp_unit_data->auto_battle_target;
        if (target == g_battle_ai_data_base.acting_unit_id
            || ((ai->unit_decisions[target].targeting_flags_2 >> 4) & (BATTLE_AI_TARGET_UNTARGETABLE >> 4))) {
            battle_ai_clear_words((s32*)ai->tile_target_distance, sizeof(ai->tile_target_distance));
            goto prepare;
        }
    }
    if (battle_ai_fill_target_distance_grid(&ai->acting_unit_decision->target) == -1) {
        return -1;
    }
prepare:
    range = battle_ai_get_movement_range();
    /* Direct typed level indexing reverses the two address-building instructions
     * at +0xbc/+0xc0. Stage only the level base; retain the real member offset
     * and typed row/column indexing for the byte-exact match. */
    distance_level_base = ai->acting_unit_coords.bytes.elevation * sizeof(ai->tile_target_distance[0]) + (u32)ai;
    distance_rows = (u8(*)[16])(distance_level_base + (u32) & ((battle_ai_data_t*)0)->tile_target_distance);
    limit = range;
    distance = distance_rows[ai->acting_unit_coords.bytes.y][ai->acting_unit_coords.bytes.x];
    within_two_tiles = distance < 3;
    ai->movement_scenario = 0;
    if (ai->autobattle_setting == BATTLE_AI_AUTOBATTLE_RECOVERY && !within_two_tiles) {
        limit = 2;
    }
    if (ai->acting_unit_decision->targeting_flags_1 & BATTLE_AI_TARGETING_FLAG_1_REFLECTED_ACTION) {
        battle_ai_select_destination(0x7fffffff, BATTLE_AI_DESTINATION_PRIORITY_FIRST);
    } else {
        battle_ai_select_destination_with_nearest_fallback(limit);
    }
    ai->movement_scenario = 1;
    ai->candidate_coords[1] = ai->candidate_coords[0];
    if (!battle_ai_filter_reachable_tiles_by_target_distance(range)) {
        battle_ai_set_candidate_and_actor_reachable_tiles();
    }
    ai->movement_scenario = 2;
    battle_ai_transfer_halfword_values(
        (u16*)ai->reachable_tiles[2], (u16*)ai->reachable_tiles[0], sizeof(ai->reachable_tiles[0]));
    if (!ai->acting_unit->action_taken) {
        s32 closer_distance = distance - 1;
        if (closer_distance < limit) {
            limit = closer_distance;
        }
        battle_ai_select_destination_with_range_floor(limit);
    } else {
        ai->candidate_coords[2] = ai->candidate_coords[1];
    }
    ai->reachable_tiles[1][ai->acting_unit_coords.bytes.elevation][ai->acting_unit_coords.bytes.y]
        |= 0x8000 >> ai->acting_unit_coords.bytes.x;
    return 0;
}
