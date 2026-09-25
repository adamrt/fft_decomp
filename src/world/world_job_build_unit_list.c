#include "fft/world.h"

/* Declared s32 here: the target uses the returned register without the
 * re-extension an s16 prototype makes GCC emit (the callee already extends). */

enum {
    WORLD_JOB_LIST_ENTRY_FLAGGED = 0x4000,
    WORLD_JOB_LIST_MODE_INCLUDE_OTHER_UNIT_UNLOCKS = 2,
};

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
    s32 count;
    s32 job;
    s32 i;
    s32 unlocked;

    count = 0;
    world_bit_cursor_set_primary(g_world_formation_unit_pointers[unit_index]->unlocked_jobs);
    job = ((s32 (*)(s16))world_job_get_base)(unit_index);
    if (mode == WORLD_JOB_LIST_MODE_INCLUDE_OTHER_UNIT_UNLOCKS
        && job == g_world_formation_unit_pointers[unit_index]->job_id) {
        job |= WORLD_JOB_LIST_ENTRY_FLAGGED;
    }
    jobs[count++] = job;
    if (world_job_is_special_monster(g_world_formation_unit_pointers[unit_index]->job_id) == 0
        && g_world_formation_unit_pointers[unit_index]->uses_monster_skillset == 0) {
        world_bit_cursor_read_primary(1);
        for (i = 1; i < JOB_ID_GENERIC_COUNT; i++) {
            job = i + JOB_ID_SQUIRE;
            unlocked = world_bit_cursor_read_primary(1);
            if (job == JOB_ID_BARD && (g_world_formation_unit_pointers[unit_index]->gender_flags & UNIT_FLAG_FEMALE)) {
                continue;
            }
            if (job == JOB_ID_DANCER && (g_world_formation_unit_pointers[unit_index]->gender_flags & UNIT_FLAG_MALE)) {
                continue;
            }
            if (mode == WORLD_JOB_LIST_MODE_INCLUDE_OTHER_UNIT_UNLOCKS
                && job == g_world_formation_unit_pointers[unit_index]->job_id) {
                jobs[count++] = job | WORLD_JOB_LIST_ENTRY_FLAGGED;
            } else if (unlocked != 0) {
                jobs[count++] = job;
            } else if (mode == WORLD_JOB_LIST_MODE_INCLUDE_OTHER_UNIT_UNLOCKS
                && world_job_is_locked_for_all_units(job) == 0) {
                jobs[count++] = job + WORLD_JOB_LIST_ENTRY_FLAGGED;
            }
        }
    }
    jobs[count] = -1;
    return count;
}
