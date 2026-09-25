#include "fft/battle.h"

/*
 * Update team-dependent AI targeting flags.
 *
 * Retreat support requires at least 2 eligible non-enemies, including one
 * neither Critical nor Frog. The unavailable-team-support flag propagates to
 * all cached non-enemies, not just the units admitted to those counts.
 */
void battle_ai_update_team_targeting_flags(void) {
    battle_ai_unit_decision_t* acting_decision = g_battle_ai_acting_unit_decision_ptr;
    battle_ai_data_t* ai = &g_battle_ai_data_base;
    s32 ally_count;
    s32 enemy_count;
    s32 impaired_ally_count;
    s32 i;

    g_battle_ai_data_base.targetable_override = 0;
    if ((acting_decision->targeting_flags_2 & BATTLE_AI_TARGET_TEAM_SUPPORT_UNAVAILABLE)
        || (g_battle_ai_temp_unit_data->status_sets.current[4]
            & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_DEATH_SENTENCE))) {
        g_battle_ai_data_base.targetability.live.target_setting_flags
            &= ~BATTLE_AI_TARGET_SETTING_RETREAT_SUPPORT_AVAILABLE;
        return;
    }
    ally_count = 0;
    enemy_count = 0;
    impaired_ally_count = 0;
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        if (battle_ai_check_target_type(i)) {
            continue;
        }
        if (ai->unit_decisions[i].enemy_flag) {
            enemy_count++;
        } else {
            ally_count++;
            if (g_battle_unit_stats[i].status_sets.current[2]
                & (BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_FROG)
                    | BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_CRITICAL))) {
                impaired_ally_count++;
            }
        }
    }
    if (ally_count == impaired_ally_count || ally_count < 2) {
        if (ai->acting_unit_team) {
            for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
                if (!ai->unit_decisions[i].enemy_flag) {
                    ai->unit_decisions[i].targeting_flags_2 |= BATTLE_AI_TARGET_TEAM_SUPPORT_UNAVAILABLE;
                }
            }
        }
        ai->targetability.live.target_setting_flags &= ~BATTLE_AI_TARGET_SETTING_RETREAT_SUPPORT_AVAILABLE;
    } else {
        if (enemy_count == 1) {
            ai->targetable_override = 1;
        }
        ai->targetability.live.target_setting_flags |= BATTLE_AI_TARGET_SETTING_RETREAT_SUPPORT_AVAILABLE;
    }
}
