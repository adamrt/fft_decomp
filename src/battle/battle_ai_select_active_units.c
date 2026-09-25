#include "fft/battle.h"

/*
 * Select active units for the AI ability search.
 *
 * Rebuild all per-unit flags and return whether any unit was selected.
 * target_id == 0xff excludes the acting unit. Confusion bypasses decision
 * flag 0x80, but does not bypass untargetability. Unknown modes allow either side.
 */
s32 battle_ai_select_active_units(battle_ai_unit_filter_e mode, s32 target_id) {
    s32 any_active = 0;
    s32 i;
    battle_ai_data_t* ai = &g_battle_ai_data_base;
    battle_ai_unit_decision_t* decision;
    battle_stats_t* unit;

    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        decision = &ai->unit_decisions[i];
        ai->targetability.live.unit_active[i] = 0;
        if (decision->targeting_flags_2 & BATTLE_AI_TARGET_UNTARGETABLE)
            continue;
        if (!(g_battle_unit_stats[i].status_sets.current[1] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_CONFUSION))
            && !(decision->flags & 0x80))
            continue;
        if (i == ai->acting_unit_id && target_id == 0xff)
            continue;
        /* Target dispatch uses a signed comparison, including unknown modes. */
        switch ((s32)mode) {
        case BATTLE_AI_UNIT_FILTER_SPECIFIC:
            if (i != target_id)
                continue;
            break;
        case BATTLE_AI_UNIT_FILTER_ENEMIES:
            if (!decision->enemy_flag)
                continue;
            break;
        case BATTLE_AI_UNIT_FILTER_ALLIES:
            if (decision->enemy_flag)
                continue;
            break;
        case BATTLE_AI_UNIT_FILTER_STATUS:
            unit = &g_battle_unit_stats[i];
            /* Target lw at 0x80197e10 reads the aligned first four status bytes. */
            if (!(*(u32*)&unit->status_sets.current[0]
                    & (BATTLE_STATUS_PACKED_MASK(BATTLE_STATUS_ID_DEAD)
                        | BATTLE_STATUS_PACKED_MASK(BATTLE_STATUS_ID_PETRIFY)
                        | BATTLE_STATUS_PACKED_MASK(BATTLE_STATUS_ID_CRITICAL)))
                && !(unit->status_sets.current[4] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_DEATH_SENTENCE)))
                continue;
            break;
        }
        ai->targetability.live.unit_active[i] = 1;
        any_active = 1;
    }
    return any_active;
}
