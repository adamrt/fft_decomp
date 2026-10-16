/* Unit job data copies and init, the base job and the unit job list. */

#include "fft/bits.h"
#include "fft/data.h"
#include "fft/event_jobstts.h"
#include "fft/main.h"
#include "fft/unit.h"
#include "psx/libc.h"
#include "psx/types.h"

/* Copy the battle unit's job data into the compact JOBSTTS working record.
 *
 * Player-controlled units and special monsters preserve the stored unlock
 * bits. Other units recalculate them from job levels and gender. Job-point
 * totals are capped to the four-digit value supported by the menu. */
static void jobstts_unit_copy_job_data(unit_stats_t* unit, jobstts_unit_job_data_t* out) {
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

/* Copy battle unit `unit_id`'s job data into the overlay's single unit
 * record and make it unit 0. */
void jobstts_unit_init_job_data(s32 unit_id) {
    jobstts_unit_copy_job_data(battle_unit_get_stats_from_battle_id(unit_id), &g_jobstts_unit_job_data);
    g_jobstts_unit_data[0] = &g_jobstts_unit_job_data;
}

/* The job that heads a unit's job list: a monster's own job, Squire for a
 * generic human identity, and for a special character the identity value
 * itself, which is that character's job id. */
static s32 jobstts_job_get_base(s16 unit_id) {
    jobstts_unit_job_data_t* unit = g_jobstts_unit_data[unit_id];
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
 * count. JOBSTTS copy of world_job_build_unit_list without its list modes.
 *
 * The base job comes first, followed by the generic jobs whose unlocked bit
 * is set (Bard excluded for female units, Dancer for male units); special
 * monsters and units with the monster identity get only the base job. The
 * third argument is not read.
 */
s32 jobstts_job_build_unit_job_list(u8 unit_id, s16* job_ids, s32 unused) {
    u32 unit_index = unit_id;
    jobstts_unit_job_data_t** unit_table = g_jobstts_unit_data;
    s16* out;
    s32 count;
    s32 i;
    s32 job;
    jobstts_unit_job_data_t** slot;
    jobstts_unit_job_data_t** loop_slot;

    (void)unused;

    slot = &unit_table[unit_index];
    bits_init_primary_reader_variant((*slot)->career.unlocked_jobs);
    job_ids[0] = jobstts_job_get_base(unit_id);
    if ((job_is_special_monster((*slot)->job_id) != 0) || ((*slot)->character_identity == CHARACTER_IDENTITY_MONSTER)) {
        job_ids[1] = -1;
        return 1;
    }
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
