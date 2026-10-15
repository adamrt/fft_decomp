#include "fft/event_jobstts.h"
#include "fft/main.h"
#include "psx/types.h"

/* Copy the battle unit's job data into the compact JOBSTTS working record.
 *
 * Player-controlled units and special monsters preserve the stored unlock
 * bits. Other units recalculate them from job levels and gender. Job-point
 * totals are capped to the four-digit value supported by the menu. */
void jobstts_unit_copy_job_data(unit_stats_t* unit, jobstts_unit_job_data_t* out) {
    s32 i;
    s32 kind;
    s32 flags;

    out->job_id = unit->job_id;
    out->unit_flags = unit->unit_flags;
    out->entd_slot = unit->entd_slot;
    out->character_identity = unit->character_identity;
    out->primary_skillset = unit->primary_skillset;

    flags = unit->initial_team_flags;
    kind = (flags & BATTLE_TEAM_MASK) != BATTLE_TEAM_BLUE;
    if (!(flags & BATTLE_TEAM_OR_PLAYER_CONTROL_MASK)) {
        kind = 2;
    }

    if ((kind == 1) || job_is_special_monster(unit->job_id)) {
        bcopy(unit->career.unlocked_jobs, out->career.unlocked_jobs, 3);
    } else {
        i = main_job_calculate_unlocked(unit->career.job_levels, unit->unit_flags & UNIT_FLAG_MALE);
        out->career.unlocked_jobs[0] = i >> 16;
        out->career.unlocked_jobs[1] = i >> 8;
        out->career.unlocked_jobs[2] = i;
    }

    bcopy(unit->career.learned_abilities, out->career.learned_abilities, 0x39);
    bcopy(unit->career.job_levels, out->career.job_levels, 0xA);
    bcopy(&unit->career.job_points.current, &out->career.job_points.current, 0x28);
    bcopy(&unit->career.job_points.total, &out->career.job_points.total, 0x28);

    for (i = 0; i < 20; i++) {
        if (out->career.job_points.current.values[i] > UNIT_JOB_POINT_MAX) {
            out->career.job_points.current.values[i] = UNIT_JOB_POINT_MAX;
        }
        if (out->career.job_points.total.values[i] > UNIT_JOB_POINT_MAX) {
            out->career.job_points.total.values[i] = UNIT_JOB_POINT_MAX;
        }
    }
}
