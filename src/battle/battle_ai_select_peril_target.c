#include "fft/battle.h"

/*
 * Select the allied target with the highest recovery-priority score.
 *
 * Keep the actor as fallback and retain the earlier slot on equal scores.
 * Status-helper checks require the actor's aggregate cancellation capability;
 * the HP-recovery flag enables dead-with-Reraise, Critical and low-HP scoring.
 * Return 1 only when the best signed score exceeds 0x02000000.
 */
s32 battle_ai_select_peril_target(void) {
    battle_ai_data_t* ai = &g_battle_ai_data_base;
    s32 best_score = 0;
    s32 i;

    g_battle_ai_temp_unit_data->auto_battle_target = g_battle_ai_data_base.acting_unit_id;
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        battle_stats_t* unit = &g_battle_unit_stats[i];
        battle_ai_unit_decision_t* decision = &ai->unit_decisions[i];
        s32 distance_score, missing_hp, team_bits, status_score, score;

        if (decision->targeting_flags_2 & BATTLE_AI_TARGET_UNTARGETABLE) {
            continue;
        }
        if (decision->enemy_flag) {
            continue;
        }
        distance_score = 255 - battle_ai_calculate_distance_between_units(ai->acting_unit, unit);
        missing_hp = 128 - ((unit->hp << 7) / unit->max_hp);
        team_bits = unit->team_flags & 3;
        if (battle_ai_can_cancel_current_status(unit, BATTLE_STATUS_ID_DEAD)
            && (decision->targeting_flags_2 & BATTLE_AI_TARGET_DEAD_WITHOUT_RERAISE)) {
            status_score = 0x78;
        } else if ((ai->acting_unit_decision->targeting_flags_1 & 2)
            && ((decision->targeting_flags_2 & BATTLE_AI_TARGET_DEAD_WITH_RERAISE)
                || (unit->status_sets.current[2] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_CRITICAL)))) {
            status_score = 0x77;
        } else if (battle_ai_can_cancel_current_status(unit, BATTLE_STATUS_ID_BLOOD_SUCK)) {
            status_score = 0x76;
        } else if (battle_ai_can_cancel_current_status(unit, BATTLE_STATUS_ID_PETRIFY)) {
            status_score = 0x75;
        } else if (battle_ai_can_cancel_current_status(unit, BATTLE_STATUS_ID_CONFUSION)) {
            status_score = 0x74;
        } else if (battle_ai_can_cancel_current_status(unit, BATTLE_STATUS_ID_FROG)) {
            status_score = 0x73;
        } else {
            status_score = 0;
            if (ai->acting_unit_decision->targeting_flags_1 & 2) {
                status_score = -(decision->targeting_flags_2 & BATTLE_AI_TARGET_HP_BELOW_HALF) & 0x72;
            }
        }
        if (status_score == 0) {
            if (decision->targeting_flags_1 & 4) {
                continue;
            }
            if (ai->acting_unit_id == i) {
                distance_score = 0;
            }
            score = (missing_hp << 16) + (team_bits << 8) + distance_score;
        } else {
            score = (status_score << 24) + (distance_score << 16) + (missing_hp << 8) + team_bits;
        }
        if (best_score < score) {
            best_score = score;
            ai->acting_unit->auto_battle_target = i;
        }
    }
    /* Clear the unit's special targeting behavior; this does not impose a distance limit. */
    ai->acting_unit_decision->flags &= ~BATTLE_AI_DECISION_SPECIAL_BEHAVIOR;
    return best_score > 0x02000000;
}
