#include "fft/battle.h"

/*
 * Refresh cached unit decision flags before AI simulation.
 *
 * Keep the simulation flag set across suspension; clear it on completion.
 */
s32 battle_ai_refresh_unit_decision_flags(void) {
    battle_ai_data_t* ai = &g_battle_ai_data_base;
    battle_stats_t* unit;
    battle_ai_unit_decision_t* decision;
    s32 unit_id;

    if (g_battle_ai_data_base.decision_state)
        goto simulate; /* resume the suspended simulation */
    for (unit_id = 0; unit_id < BATTLE_UNIT_SLOT_COUNT; unit_id++) {
        unit = &g_battle_unit_stats[unit_id];
        decision = &ai->unit_decisions[unit_id];
        if (battle_ai_can_unit_be_targeted_cryst_trea_mount_trans(unit_id)) {
            /* Other cached fields are intentionally untouched for excluded units. */
            decision->targeting_flags_2 |= BATTLE_AI_TARGET_UNTARGETABLE;
            continue;
        }
        if (unit_id != ai->acting_unit_id && (unit->initial_team_flags & BATTLE_TEAM_MASK) != ai->acting_unit_team)
            decision->enemy_flag = 1;
        else
            decision->enemy_flag = 0;
        decision->targeting_flags_2 &= BATTLE_AI_TARGET_CONSERVE_CT | BATTLE_AI_TARGET_TEAM_SUPPORT_UNAVAILABLE;
        if (unit->status_sets.current[0] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_DEAD)) {
            if (unit->status_sets.current[2] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_RERAISE))
                decision->targeting_flags_2 |= BATTLE_AI_TARGET_DEAD_WITH_RERAISE;
            else
                decision->targeting_flags_2 |= BATTLE_AI_TARGET_DEAD_WITHOUT_RERAISE;
        }
        if (unit->hp < (u32)unit->max_hp / 2)
            decision->targeting_flags_2 |= BATTLE_AI_TARGET_HP_BELOW_HALF;
        if (decision->mp_ability_mod) {
            if (unit->mp < (u32)unit->max_mp / 2)
                decision->targeting_flags_2 |= BATTLE_AI_TARGET_MP_CONSTRAINED;
            if (ai->acting_unit_team && unit->mp < (u32)decision->highest_mp_cost)
                decision->targeting_flags_2 |= BATTLE_AI_TARGET_MP_CONSTRAINED;
        }
        decision->targeting_flags_2 &= ~BATTLE_AI_TARGET_HP_BELOW_THREE_QUARTERS;
        /* Cross-multiply the strict three-quarter HP threshold. */
        if (unit->hp * 4 < unit->max_hp * 4 - unit->max_hp)
            decision->targeting_flags_2 |= BATTLE_AI_TARGET_HP_BELOW_THREE_QUARTERS;
        decision->flags &= 0x7f;
        if (!decision->enemy_flag || unit->charged_ability_ct != 0xff)
            decision->flags |= 0x80;
    }
    ai->current_action.target_flags_set = 1;
    ai->current_action.skillset = 0;
    ai->targetability.live.target_setting_flags |= BATTLE_AI_TARGET_SETTING_REFRESH_UNIT_STATUS_FLAGS;
simulate:
    switch (battle_ai_simulate_action_and_score()) {
    case -1:
        return -1;
    default:
        ai->targetability.live.target_setting_flags &= ~BATTLE_AI_TARGET_SETTING_REFRESH_UNIT_STATUS_FLAGS;
        return 0;
    }
}
