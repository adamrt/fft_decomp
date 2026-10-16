/* The list of jobs a formation unit can select, optionally with jobs other units have unlocked. */

#include "fft/bits.h"
#include "fft/data_job.h"
#include "fft/job.h"
#include "fft/main.h"
#include "fft/unit.h"
#include "fft/world_formation.h"
#include "psx/types.h"

/* Declared s32 here: the target uses the returned register without the
 * re-extension an s16 prototype makes GCC emit (the callee already extends). */

/*
 * Writes a formation unit's selectable jobs to a -1-terminated list and
 * returns their count.
 *
 * The base job comes first, followed by the generic jobs whose unlocked bit
 * is set (Bard excluded for female units, Dancer for male units); special
 * monsters and units using monster skillsets get only the base job. Mode 2
 * marks the current job and also lists jobs another unit has unlocked, with
 * the 0x4000 entry flag.
 */
s32 world_job_build_unit_list(u8 unit_index, s16* jobs, s32 mode) {
    s32 count = 0;
    s32 job;
    s32 i;
    s32 unlocked;

    bits_init_primary_reader(g_world_unit_pointers[unit_index]->unlocked_jobs);
    job = world_job_get_base(unit_index);
    if (mode == WORLD_JOB_LIST_MODE_INCLUDE_OTHER_UNIT_UNLOCKS
        && job == g_world_unit_pointers[unit_index]->identity.job_id) {
        job |= WORLD_JOB_LIST_ENTRY_UNAVAILABLE;
    }
    jobs[count++] = job;
    if (job_is_special_monster(g_world_unit_pointers[unit_index]->identity.job_id) == 0
        && g_world_unit_pointers[unit_index]->uses_monster_skillset == 0) {
        bits_read_primary(1);
        for (i = 1; i < JOB_ID_GENERIC_COUNT; i++) {
            job = i + JOB_ID_SQUIRE;
            unlocked = bits_read_primary(1);
            if (job == JOB_ID_BARD && (g_world_unit_pointers[unit_index]->gender_flags & UNIT_FLAG_FEMALE)) {
                continue;
            }
            if (job == JOB_ID_DANCER && (g_world_unit_pointers[unit_index]->gender_flags & UNIT_FLAG_MALE)) {
                continue;
            }
            if (mode == WORLD_JOB_LIST_MODE_INCLUDE_OTHER_UNIT_UNLOCKS
                && job == g_world_unit_pointers[unit_index]->identity.job_id) {
                jobs[count++] = job | WORLD_JOB_LIST_ENTRY_UNAVAILABLE;
            } else if (unlocked != 0) {
                jobs[count++] = job;
            } else if (mode == WORLD_JOB_LIST_MODE_INCLUDE_OTHER_UNIT_UNLOCKS
                && world_job_is_locked_for_all_units(job) == 0) {
                jobs[count++] = job + WORLD_JOB_LIST_ENTRY_UNAVAILABLE;
            }
        }
    }
    jobs[count] = -1;
    return count;
}
