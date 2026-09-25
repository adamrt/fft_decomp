#include "fft/battle.h"

/*
 * Return whether the first action outranks the second.
 *
 * Rank ties prefer lower foe proximity; equal proximity is not an improvement.
 * Reusing the working scalar for the level preserves target register lifetimes.
 */
s32 battle_ai_is_action_higher_ranked(battle_ai_action_data_t* first, battle_ai_action_data_t* second) {
    battle_ai_data_t* ai = &g_battle_ai_data_base;
    battle_ai_rank_order_e comparison;
    s32 proximity_a;
    s32 proximity_b;
    s32 value;

    comparison = battle_ai_compare_target_priority_and_hit_rate(
        (battle_ai_action_rank_t*)&first->rank_byte, (battle_ai_action_rank_t*)&second->rank_byte);
    value = 1;
    if (comparison == BATTLE_AI_RANK_ABOVE)
        return value;
    if (comparison == BATTLE_AI_RANK_EQUAL) {
        value = first->coords.bytes.elevation;
        proximity_a = ai->tile_foe_proximity[value][first->coords.bytes.y][first->coords.bytes.x];
        proximity_b
            = ai->tile_foe_proximity[second->coords.bytes.elevation][second->coords.bytes.y][second->coords.bytes.x];
        return proximity_a < proximity_b;
    }
    return 0;
}
