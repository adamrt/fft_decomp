/* Unit job data copies and init, the base job and the unit job list. */

#include "fft/battle_unit.h"
#include "fft/bits.h"
#include "fft/data_job.h"
#include "fft/event_jobstts.h"
#include "fft/job.h"
#include "fft/main_unit.h"
#include "fft/unit.h"
#include "fft/unit_identity.h"
#include "psx/libc.h"
#include "psx/types.h"

static status_screen_unit_jobs_t g_status_screen_unit_job_data = { 0 };

/* Copy the battle unit's job data into the compact JOBSTTS working record.
 *
 * Units on a team other than the player's, and special monsters, keep their stored
 * unlock bits; the player's team (team_kind 0, or 2 for a guest without the
 * player-control flag) has them recalculated from job levels and gender. Current and
 * total JP are capped at UNIT_JOB_POINT_MAX, the four digits the menu shows. */
static void status_screen_copy_unit_jobs(unit_stats_t* unit, status_screen_unit_jobs_t* out) {
    s32 i;
    s32 team_kind;
    s32 flags;

    out->job_id = unit->job_id;
    out->unit_flags = unit->unit_flags;
    out->entd_slot = unit->entd_slot;
    out->character_identity = unit->character_identity;
    out->primary_skillset = unit->primary_skillset;

    flags = unit->initial_team_flags;
    team_kind = (flags & UNIT_TEAM_MASK) != UNIT_TEAM_BLUE;
    if (!(flags & UNIT_TEAM_OR_PLAYER_CONTROL_MASK)) {
        team_kind = 2;
    }

    if ((team_kind == 1) || job_is_special_monster(unit->job_id)) {
        bcopy(unit->career.unlocked_jobs, out->career.unlocked_jobs, sizeof(out->career.unlocked_jobs));
    } else {
        i = job_calculate_unlocked(unit->career.job_levels, unit->unit_flags & UNIT_FLAG_MALE);
        out->career.unlocked_jobs[0] = i >> 16;
        out->career.unlocked_jobs[1] = i >> 8;
        out->career.unlocked_jobs[2] = i;
    }

    bcopy(unit->career.learned_abilities, out->career.learned_abilities, sizeof(out->career.learned_abilities));
    bcopy(unit->career.job_levels, out->career.job_levels, sizeof(out->career.job_levels));
    bcopy(&unit->career.job_points.current, &out->career.job_points.current, sizeof(out->career.job_points.current));
    bcopy(&unit->career.job_points.total, &out->career.job_points.total, sizeof(out->career.job_points.total));

    for (i = 0; i < UNIT_CAREER_JOB_COUNT; i++) {
        if (out->career.job_points.current.values[i] > UNIT_JOB_POINT_MAX) {
            out->career.job_points.current.values[i] = UNIT_JOB_POINT_MAX;
        }
        if (out->career.job_points.total.values[i] > UNIT_JOB_POINT_MAX) {
            out->career.job_points.total.values[i] = UNIT_JOB_POINT_MAX;
        }
    }
}

/* Copy battle unit `unit_id`'s job data into the overlay's single unit
 * record and make it unit 0. */
void status_screen_init_unit_jobs(s32 unit_id) {
    status_screen_copy_unit_jobs(unit_get_stats(unit_id), &g_status_screen_unit_job_data);
    g_status_screen_unit_data[0] = &g_status_screen_unit_job_data;
}

/* The job that heads a unit's job list: a monster's own job, Squire for a
 * generic human identity, and for a special character the identity value
 * itself, which is that character's job id. */
static s32 job_list_get_base(s16 unit_id) {
    status_screen_unit_jobs_t* unit = g_status_screen_unit_data[unit_id];
    u32 job_id = unit->character_identity;

    if (job_id == CHARACTER_IDENTITY_MONSTER) {
        job_id = unit->job_id;
    } else if (job_id >= CHARACTER_IDENTITY_GENERIC_MALE) {
        job_id = JOB_ID_SQUIRE;
    }
    return job_id;
}

/*
 * Writes a unit's selectable jobs to a -1-terminated list and returns their
 * count. JOBSTTS copy of job_list_build without its list modes.
 *
 * The base job comes first, followed by the generic jobs whose unlocked bit
 * is set (Bard excluded for female units, Dancer for male units); special
 * monsters and units with the monster identity get only the base job. The
 * third argument is not read.
 */
s32 job_list_build(u8 unit_id, s16* job_ids, s32 unused) {
    u32 unit_index = unit_id;
    status_screen_unit_jobs_t** unit_table = g_status_screen_unit_data;
    s16* out;
    s32 count;
    s32 i;
    s32 job;
    status_screen_unit_jobs_t** slot;
    status_screen_unit_jobs_t** loop_slot;

    (void)unused;

    slot = &unit_table[unit_index];
    bits_init_primary_reader((*slot)->career.unlocked_jobs);
    job_ids[0] = job_list_get_base(unit_id);
    if ((job_is_special_monster((*slot)->job_id) != 0) || ((*slot)->character_identity == CHARACTER_IDENTITY_MONSTER)) {
        job_ids[1] = -1;
        return 1;
    }
    /* Skip Squire's bit: the base job above takes Squire's place. */
    bits_read_primary(1);
    count = 1;
    i = 1;
    loop_slot = slot;
    out = job_ids + 1;
    for (; i < JOB_ID_GENERIC_COUNT; i++) {
        job = i + JOB_ID_SQUIRE;
        if ((bits_read_primary(1) != 0) && ((job != JOB_ID_BARD) || !((*loop_slot)->unit_flags & UNIT_FLAG_FEMALE))
            && ((job != JOB_ID_DANCER) || !((*loop_slot)->unit_flags & UNIT_FLAG_MALE))) {
            *out = job;
            out++;
            count++;
        }
    }
    job_ids[count] = -1;
    return count;
}
