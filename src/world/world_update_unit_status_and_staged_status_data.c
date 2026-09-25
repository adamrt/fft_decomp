#include "fft/battle.h"
#include "fft/world.h"

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
s32 world_update_unit_status_and_staged_status_data(s32 unit_id) {
    battle_stats_t* stats;
    s32 i;
    s32 value;
    unit_status_staging_t* st;

    if (g_world_unit_status_staging_data->state[unit_id] == 0) {
        stats = battle_unit_get_stats_from_battle_id(unit_id);
        for (i = 0; i < BATTLE_STATUS_BYTE_COUNT; i++) {
            g_world_unit_status_staging_data->innate[unit_id][i] = stats->status_sets.innate[i];
            g_world_unit_status_staging_data->inflicted[unit_id][i] = stats->inflicted_status[i];
            g_world_unit_status_staging_data->current[unit_id][i] = stats->status_sets.current[i];
        }
        for (i = 0; i < BATTLE_TIMED_STATUS_COUNT; i++) {
            g_world_unit_status_staging_data->status_ct[unit_id][i] = stats->status_ct[i];
        }
        g_world_unit_status_staging_data->charged_ability_ct[unit_id] = stats->charged_ability_ct;
        g_world_unit_status_staging_data->death_counter[unit_id] = stats->death_counter;
        g_world_unit_status_staging_data->hp[unit_id] = stats->hp;
        g_world_unit_status_staging_data->team_flags[unit_id] = stats->team_flags;
        for (i = 0; i < BATTLE_STATUS_BYTE_COUNT; i++) {
            if (world_script_get_variable(EVENT_SCRIPT_VAR_PENDING_STAGED_STATUS) != 0) {
                if ((stats->primary_skillset >= SKILLSET_ID_MONSTER_FIRST && stats->primary_skillset < SKILLSET_ID_END)
                    || stats->character_identity == CHARACTER_IDENTITY_ALTIMA_FIRST_FORM
                    || stats->character_identity == CHARACTER_IDENTITY_ALTIMA_SECOND_FORM
                    || stats->job_id == JOB_ID_KNIGHT_UNDEAD || stats->job_id == JOB_ID_ARCHER_UNDEAD
                    || stats->job_id == JOB_ID_WIZARD_UNDEAD || stats->job_id == JOB_ID_TIME_MAGE_UNDEAD
                    || stats->job_id == JOB_ID_ORACLE_UNDEAD || stats->job_id == JOB_ID_SUMMONER_UNDEAD) {
                    value = stats->status_sets.current[i];
                    value &= g_world_status_removal_mask_staged_monster[i];
                    st = g_world_unit_status_staging_data;
                    stats->action.status_removal[i] = value;
                    st->added[unit_id][i] = value;
                    st->removed[unit_id][i] = value;
                } else {
                    value = stats->status_sets.current[i];
                    value &= g_world_status_removal_mask_staged[i];
                    st = g_world_unit_status_staging_data;
                    stats->action.status_removal[i] = value;
                    st->added[unit_id][i] = value;
                    st->removed[unit_id][i] = value;
                }
            } else {
                value = stats->status_sets.current[i];
                value &= g_world_status_removal_mask_unstaged[i];
                st = g_world_unit_status_staging_data;
                stats->action.status_removal[i] = value;
                st->added[unit_id][i] = value;
                st->removed[unit_id][i] = value;
            }
        }
        if (g_world_unit_status_staging_data->removed[unit_id][BATTLE_STATUS_BYTE_INDEX(BATTLE_STATUS_ID_JUMP)]
            & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_JUMP)) {
            g_world_unit_status_staging_data->flags |= 1;
        }
        if (g_world_unit_status_staging_data->removed[unit_id][BATTLE_STATUS_BYTE_INDEX(BATTLE_STATUS_ID_FLOAT)]
            & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_FLOAT)) {
            g_world_unit_status_staging_data->flags |= 2;
        }
        if (stats->action.status_removal[BATTLE_STATUS_BYTE_INDEX(BATTLE_STATUS_ID_CHARM)]
            & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_CHARM)) {
            stats->team_flags = stats->initial_team_flags;
        }
        g_world_thread_call_target = (void (*)(void))battle_status_apply_unit_action_removal;
        world_thread_call_on_main_stack(stats);
        g_world_unit_status_staging_data->state[unit_id] = 1;
        return 1;
    }
    return 0;
}
