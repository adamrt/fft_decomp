#include "fft/battle.h"

/* Evaluate the actor's abilities for removing one status from a target.
 *
 * The return value distinguishes suspension (-1), no usable ability (0), a
 * retry-worthy result (1), and a selected action (2). The Blood Suck path
 * deliberately tests the transient Jump flag, preserving the shipped AI
 * quirk.
 */
s32 battle_ai_evaluate_status_cancellation(s32 unit_id, s32 status_id) {
    battle_ai_data_t* ai = &g_battle_ai_data_base;
    battle_stats_t* unit = &g_battle_unit_stats[unit_id];
    battle_ai_unit_decision_t* decision;
    battle_ai_ability_entry_t* entry;
    s32 flags;
    s32 status_byte;
    /* Pin: unpinned, GCC builds the result in $v1 and copies it to $v0 at the returns (4 bytes larger). */
    register s32 result __asm__("$2");

    if (g_battle_ai_data_base.decision_state != 0) {
        /* Resume: the target jumps into the middle of the ability loop. */
        goto evaluate_ability;
    }

    decision = &ai->unit_decisions[unit_id];
    g_battle_ai_status_candidate_count = 0;
    g_battle_ai_status_insufficient_mp = 0;
    g_battle_ai_status_sufficient_mp = 0;

    switch (status_id) {
    case BATTLE_STATUS_ID_DEAD:
        if (!(decision->flags & BATTLE_AI_DECISION_TRANSIENT_DEAD))
            return 1;
        break;
    case BATTLE_STATUS_ID_PETRIFY:
        if (!(decision->flags & BATTLE_AI_DECISION_TRANSIENT_PETRIFIED))
            return 1;
        break;
    case BATTLE_STATUS_ID_BLOOD_SUCK:
        if (!(decision->flags & BATTLE_AI_DECISION_TRANSIENT_JUMP))
            return 1;
        break;
    }

    if (battle_ai_select_active_units(BATTLE_AI_UNIT_FILTER_SPECIFIC, unit_id) != 0) {
        for (ai->ability_counter = 0; ai->ability_counter < 0x22; ai->ability_counter++) {
            entry = &ai->ability_lists[ai->acting_unit_battle_id][ai->ability_counter];
            if (entry->skillset_flags.bytes.skillset == SKILLSET_ID_NONE)
                break;
            battle_ai_load_ability_entry(entry);
            flags = ai->considered_ability.ai_flags.word;
            if (!(flags & BATTLE_AI_ABILITY_CANCEL_STATUS))
                continue;

            status_byte = status_id;
            if (status_id < 0)
                status_byte = status_id + 7;
            status_byte >>= 3;
            if (!(ai->considered_ability.parameters.bytes.status_infliction[status_byte]
                    & (0x80 >> (status_id - status_byte * 8))))
                continue;
            if ((flags & BATTLE_AI_ABILITY_AFFECTED_BY_FAITH)
                && (unit->status_sets.innate[4] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_INNOCENT)))
                continue;
            if ((flags & BATTLE_AI_ABILITY_REFLECTABLE)
                && (unit->status_sets.innate[4] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_REFLECT)))
                continue;
            if ((flags & BATTLE_AI_ABILITY_AFFECTED_BY_SILENCE)
                && (ai->acting_unit->status_sets.current[1] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_SILENCE)))
                continue;

            if (ai->acting_unit->mp < ai->considered_ability.mp_cost)
                g_battle_ai_status_insufficient_mp = 1;
            else
                g_battle_ai_status_sufficient_mp = 1;
            g_battle_ai_status_candidate_count++;

        evaluate_ability:
            if (battle_ai_evaluate_ability_outcome() == -1)
                return -1;
        }
    }

    if (ai->ranked_actions[0].base_hit_percent != 0) {
        battle_ai_transfer_halfword_values(
            (u16*)&ai->selected_action, (u16*)&ai->ranked_actions[0], sizeof(battle_ai_action_data_t));
        return 2;
    }
    if (g_battle_ai_status_candidate_count == 0)
        return 0;
    result = 1;
    if (g_battle_ai_status_insufficient_mp != 0 && ai->ability_targets_enemies_or_mp == 0)
        result = g_battle_ai_status_sufficient_mp != 0;
    return result;
}
