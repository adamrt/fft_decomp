/* Derive job levels and unlock masks from earned JP. Proposition awards feed the same
 * progression data that formation and battle unit construction use. */

#include "fft/job.h"
#include "fft/party.h"
#include "fft/unit.h"
#include "fft/unit_party.h"
#include "psx/types.h"

/* Compute a unit's unlocked-job bitset (Squire in bit 23, always set) from its packed job levels:
 * a job unlocks when every level nibble meets its g_job_unlock_requirements row. The other
 * sex's Bard or Dancer level counts as maxed for the check, then that job is masked out; a monster
 * unlocks nothing. */
u32 job_unlocks_calculate(const u8* packed_job_levels, u32 gender_flags) {
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

    unlock_bit = UNIT_UNLOCKED_JOB_SQUIRE;
    unlocked = UNIT_UNLOCKED_JOB_SQUIRE;
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
        unlocked &= UNIT_UNLOCKED_JOB_MASK & ~UNIT_UNLOCKED_JOB_DANCER;
    }
    if (gender_flags & UNIT_FLAG_FEMALE) {
        unlocked &= UNIT_UNLOCKED_JOB_MASK & ~UNIT_UNLOCKED_JOB_BARD;
    }
    return unlocked;
}

/* Awards proposition JP to a roster unit's current generic job (special jobs
 * use slot 0), recomputes the job level nibble and the unlocked-job bitset.
 * Returns -1 when the unit or JP amount is invalid. */
s32 job_proposition_jp_add(u32 party_index, s32 jp) {
    s32 slot;
    party_unit_t* party;
    u8 job;
    s32 total;
    s32 level;
    s32 levels;
    u8 level_byte;
    u16 total_jp;

    if (jp < 0) {
        return -1;
    }
    if (party_index >= PARTY_ROSTER_SLOT_COUNT) {
        return -1;
    }
    party = party_unit_get(party_index);
    if (party->party_id == PARTY_ID_NONE) {
        return -1;
    }
    if (party->gender_flags & UNIT_FLAG_MONSTER) {
        return -1;
    }
    job = party->job_id;
    if (job >= JOB_ID_GENERIC_FIRST && job <= JOB_ID_GENERIC_LAST) {
        slot = job - JOB_ID_GENERIC_FIRST;
    } else {
        slot = 0;
    }

    /* Each serialized JP entry is a little-endian byte pair. */
    total = party->career.job_points.current.entries[slot].low
        + (party->career.job_points.current.entries[slot].high << 8) + jp;
    if (total > UNIT_JOB_POINT_MAX) {
        total = UNIT_JOB_POINT_MAX;
    }
    party->career.job_points.current.entries[slot].low = total;
    party->career.job_points.current.entries[slot].high = total / 256;

    total = party->career.job_points.total.entries[slot].low + (party->career.job_points.total.entries[slot].high << 8)
        + jp;
    if (total > UNIT_JOB_POINT_MAX) {
        total = UNIT_JOB_POINT_MAX;
    }
    party->career.job_points.total.entries[slot].low = total;
    party->career.job_points.total.entries[slot].high = total / 256;
    total_jp = total;
    level = job_level_calculate(total_jp);

    levels = party->career.job_levels[slot / 2];
    level_byte = level;
    if ((slot & 1) != 0) {
        levels = level + (levels & 0xF0);
    } else {
        levels = (levels & 0xF) | (level_byte << 4);
    }
    party->career.job_levels[slot / 2] = levels;

    job_unlocks_store(
        party->career.unlocked_jobs, job_unlocks_calculate(party->career.job_levels, party->gender_flags));
    return 0;
}

/* Store the low 24 bits of unlocked_jobs as three bytes, most significant first. */
void job_unlocks_store(u8 destination[UNIT_UNLOCKED_JOB_BYTE_COUNT], u32 unlocked_jobs) {
    *destination++ = unlocked_jobs >> 16;
    *destination++ = unlocked_jobs >> 8;
    *destination = unlocked_jobs;
}

/* Job level: how many of the eight thresholds in
 * g_job_jp_requirements_by_level[1..8] the given JP has reached (0..8). */
u32 job_level_calculate(u16 job_jp) {
    s32 job_level = 0;
    s32 level;

    for (level = 0; level < UNIT_JOB_LEVEL_MAX; level++) {
        if (job_jp >= g_job_jp_requirements_by_level[level + 1]) {
            job_level++;
        }
    }
    return job_level;
}

/* Pack each pair of job levels from the jobs' total JP: the first job of a pair in the high
 * nibble, the second in the low. */
void unit_job_levels_init(const unit_job_point_table_t* unit_job_jp, u8 unit_job_levels[UNIT_JOB_LEVEL_BYTE_COUNT]) {
    s32 i;
    s32 level;

    for (i = 0; i < UNIT_JOB_LEVEL_BYTE_COUNT; i++) {
        level = job_level_calculate(unit_job_jp->values[i * UNIT_JOBS_PER_LEVEL_BYTE]) << 4;
        unit_job_levels[i] = level | job_level_calculate(unit_job_jp->values[i * UNIT_JOBS_PER_LEVEL_BYTE + 1]);
    }
}
