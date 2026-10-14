#include "fft/battle.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "fft/unit.h"

/* Snapshot a unit into the event staging record and queue its status removal.
 *
 * Returns 0 when the slot is already staged. Otherwise the unit's statuses,
 * timers, HP and team are saved, the removal mask is chosen by the pending
 * staged-status variable and the unit type (monsters, Altima and the undead
 * jobs use their own mask), and the removal is applied on the main stack.
 *
 * Each mask branch repeats the whole store tail; cross-jumping merges the
 * copies, and the repeated tail uses give the strength-reduced status walker
 * the target's s1 over the stats pointer. */
s32 unit_save_staged_status_data(s32 battle_id) {
    unit_stats_t* stats;
    s32 i;
    s32 value;
    unit_status_staging_t* st;

    if (g_unit_status_staging_data->state[battle_id] == UNIT_STATUS_STAGING_NONE) {
        stats = battle_unit_get_stats_from_battle_id(battle_id);
        for (i = 0; i < STATUS_BYTE_COUNT; i++) {
            g_unit_status_staging_data->innate[battle_id][i] = stats->status_sets.innate[i];
            g_unit_status_staging_data->inflicted[battle_id][i] = stats->inflicted_status[i];
            g_unit_status_staging_data->current[battle_id][i] = stats->status_sets.current[i];
        }
        for (i = 0; i < BATTLE_TIMED_STATUS_COUNT; i++) {
            g_unit_status_staging_data->status_ct[battle_id][i] = stats->status_ct[i];
        }
        g_unit_status_staging_data->charged_ability_ct[battle_id] = stats->charged_ability_ct;
        g_unit_status_staging_data->death_counter[battle_id] = stats->death_counter;
        g_unit_status_staging_data->hp[battle_id] = stats->hp;
        g_unit_status_staging_data->team_flags[battle_id] = stats->team_flags;
        for (i = 0; i < STATUS_BYTE_COUNT; i++) {
            if (script_get_variable(EVENT_SCRIPT_VAR_BATTLE_NOT_STARTED) != 0) {
                if ((stats->primary_skillset >= SKILLSET_ID_MONSTER_FIRST && stats->primary_skillset < SKILLSET_ID_END)
                    || stats->character_identity == CHARACTER_IDENTITY_ALTIMA_FIRST_FORM
                    || stats->character_identity == CHARACTER_IDENTITY_ALTIMA_SECOND_FORM
                    || stats->job_id == JOB_ID_KNIGHT_UNDEAD || stats->job_id == JOB_ID_ARCHER_UNDEAD
                    || stats->job_id == JOB_ID_WIZARD_UNDEAD || stats->job_id == JOB_ID_TIME_MAGE_UNDEAD
                    || stats->job_id == JOB_ID_ORACLE_UNDEAD || stats->job_id == JOB_ID_SUMMONER_UNDEAD) {
                    value = stats->status_sets.current[i];
                    value &= g_unit_status_staging_masks[UNIT_STATUS_STAGING_MASK_PRE_BATTLE_MONSTER_OR_UNDEAD][i];
                    st = g_unit_status_staging_data;
                    stats->action.status_changes.removal[i] = value;
                    st->added[battle_id][i] = value;
                    st->removed[battle_id][i] = value;
                } else {
                    value = stats->status_sets.current[i];
                    value &= g_unit_status_staging_masks[UNIT_STATUS_STAGING_MASK_PRE_BATTLE][i];
                    st = g_unit_status_staging_data;
                    stats->action.status_changes.removal[i] = value;
                    st->added[battle_id][i] = value;
                    st->removed[battle_id][i] = value;
                }
            } else {
                value = stats->status_sets.current[i];
                value &= g_unit_status_staging_masks[UNIT_STATUS_STAGING_MASK_BATTLE_STARTED][i];
                st = g_unit_status_staging_data;
                stats->action.status_changes.removal[i] = value;
                st->added[battle_id][i] = value;
                st->removed[battle_id][i] = value;
            }
        }
        if (g_unit_status_staging_data->removed[battle_id][STATUS_BYTE_INDEX(STATUS_ID_JUMP)]
            & STATUS_BYTE_MASK(STATUS_ID_JUMP)) {
            g_unit_status_staging_data->jump_float_flags |= UNIT_STATUS_STAGING_JUMP_REMOVED;
        }
        if (g_unit_status_staging_data->removed[battle_id][STATUS_BYTE_INDEX(STATUS_ID_FLOAT)]
            & STATUS_BYTE_MASK(STATUS_ID_FLOAT)) {
            g_unit_status_staging_data->jump_float_flags |= UNIT_STATUS_STAGING_FLOAT_REMOVED;
        }
        if (stats->action.status_changes.removal[STATUS_BYTE_INDEX(STATUS_ID_CHARM)]
            & STATUS_BYTE_MASK(STATUS_ID_CHARM)) {
            stats->team_flags = stats->initial_team_flags;
        }
        /* WORLD's copy targets this BATTLE address too (see QUIRKS.md). */
        g_thread_call_target = (void (*)(void))status_apply_unit_action_removal;
        thread_call_on_main_stack(stats);
        g_unit_status_staging_data->state[battle_id] = UNIT_STATUS_STAGING_SAVED;
        return 1;
    }
    return 0;
}
