#include "fft/job.h"
#include "psx/types.h"

/* Return the unlocked-job bitset for a unit's packed job levels.
 *
 * Squire (0x800000) is always unlocked; each later generic job takes the next
 * lower bit when every packed level nibble meets its requirements row.
 * Females get a full Bard nibble and males a full Dancer nibble, so the job a
 * gender cannot take never blocks Mime; that job's bit is then cleared.
 * Monsters unlock nothing.
 *
 * The level copy is sized by job count although only the packed bytes are
 * used, which gives the target's 24-byte frame. The two separate
 * `blocked = 1` statements (merged into one block by the compiler) give
 * `blocked` the allocation priority that places it in $t1.
 */
u32 main_job_calculate_unlocked(const u8* packed_job_levels, u32 gender_flags) {
    u8 levels[UNIT_CAREER_JOB_COUNT];
    s32 unlock_bit;
    s32 unlocked;
    s32 job;
    s32 i;
    s32 j;
    s32 blocked;

    for (i = 0; i < UNIT_JOB_LEVEL_BYTE_COUNT; i++) {
        levels[i] = packed_job_levels[i];
    }
    if (gender_flags & UNIT_FLAG_FEMALE) {
        levels[UNIT_JOB_LEVEL_BYTE_INDEX_CALCULATOR_BARD] |= UNIT_JOB_LEVEL_LOW_NIBBLE_MASK;
    }
    if (gender_flags & UNIT_FLAG_MALE) {
        levels[UNIT_JOB_LEVEL_BYTE_INDEX_DANCER_MIME] |= UNIT_JOB_LEVEL_HIGH_NIBBLE_MASK;
    }
    if (gender_flags & UNIT_FLAG_MONSTER) {
        return 0;
    }

    unlock_bit = 0x800000;
    unlocked = 0x800000;
    for (job = 0; job < UNIT_CAREER_JOB_COUNT - 1; job++) {
        unlock_bit /= 2;
        blocked = 0;
        for (j = 0; j < UNIT_JOB_LEVEL_BYTE_COUNT; j++) {
            if ((g_job_unlock_requirements[job][j] & UNIT_JOB_LEVEL_HIGH_NIBBLE_MASK)
                > (levels[j] & UNIT_JOB_LEVEL_HIGH_NIBBLE_MASK)) {
                blocked = 1;
                break;
            }
            if ((g_job_unlock_requirements[job][j] & UNIT_JOB_LEVEL_LOW_NIBBLE_MASK)
                > (levels[j] & UNIT_JOB_LEVEL_LOW_NIBBLE_MASK)) {
                blocked = 1;
                break;
            }
        }
        if (!blocked) {
            unlocked |= unlock_bit;
        }
    }

    if (gender_flags & UNIT_FLAG_MALE) {
        unlocked &= 0xffffd0;
    }
    if (gender_flags & UNIT_FLAG_FEMALE) {
        unlocked &= 0xffffb0;
    }
    return unlocked;
}
