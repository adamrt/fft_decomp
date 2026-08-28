#include "fft/jobstts.h"
#include "fft/main_unit.h"
#include "psx/types.h"

/* Copy the battle unit's job data into the compact JOBSTTS working record.
 *
 * Player-controlled units and special monsters preserve the stored unlock
 * bits. Other units recalculate them from job levels and gender. Job-point
 * totals are capped to the four-digit value supported by the menu. */
void jobstts_unit_copy_job_data(battle_stats_t* unit, jobstts_unit_job_data_t* out) {
    s32 i;
    s32 kind;
    s32 flags;

    out->job_id = unit->job_id;
    out->unit_flags = unit->unit_flags;
    out->entd_slot = unit->entd_slot;
    out->character_identity = unit->character_identity;
    out->primary_skillset = unit->primary_skillset;

    flags = unit->initial_team_flags;
    kind = (flags & 0x30) != 0;
    if (!(flags & 0x38)) {
        kind = 2;
    }

    if ((kind == 1) || jobstts_job_is_special_monster(unit->job_id)) {
        bcopy(unit->unlocked_jobs, out->unlocked_jobs, 3);
    } else {
        i = main_job_calculate_unlocked(unit->job_levels, unit->unit_flags & UNIT_FLAG_MALE);
        out->unlocked_jobs[0] = i >> 16;
        out->unlocked_jobs[1] = i >> 8;
        out->unlocked_jobs[2] = i;
    }

    bcopy(unit->learned_abilities, out->learned_abilities, 0x39);
    bcopy(unit->job_levels, out->job_levels, 0xA);
    bcopy(unit->job_points, out->job_points, 0x28);
    bcopy(unit->total_job_points, out->total_job_points, 0x28);

    for (i = 0; i < 20; i++) {
        if (out->job_points[i] >= 10000) {
            out->job_points[i] = 9999;
        }
        if (out->total_job_points[i] >= 10000) {
            out->total_job_points[i] = 9999;
        }
    }
}
