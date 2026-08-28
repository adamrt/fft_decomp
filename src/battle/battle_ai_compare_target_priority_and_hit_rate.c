#include "fft/battle_ai.h"

/*
 * Compare the ranking fields of two actions.
 *
 * Compare rank byte, signed priority, then hit percentage. The rank byte's
 * gameplay meaning is unresolved.
 */
battle_ai_rank_order_e battle_ai_compare_target_priority_and_hit_rate(
    battle_ai_action_rank_t* first, battle_ai_action_rank_t* second) {
    u8 rank_a, rank_b;
    s16 priority_a, priority_b;
    u8 hit_a, hit_b;

    rank_a = first->rank_byte;
    rank_b = second->rank_byte;
    if (rank_a > rank_b)
        return BATTLE_AI_RANK_ABOVE;
    if (rank_a < rank_b)
        return BATTLE_AI_RANK_BELOW;
    priority_a = first->priority;
    priority_b = second->priority;
    if (priority_a > priority_b)
        return BATTLE_AI_RANK_ABOVE;
    if (priority_a < priority_b)
        return BATTLE_AI_RANK_BELOW;
    hit_a = first->base_hit_percent;
    hit_b = second->base_hit_percent;
    if (hit_a > hit_b)
        return BATTLE_AI_RANK_ABOVE;
    if (hit_a < hit_b)
        return BATTLE_AI_RANK_BELOW;
    return BATTLE_AI_RANK_EQUAL;
}
