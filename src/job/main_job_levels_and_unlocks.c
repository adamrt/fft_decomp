/* Job level and unlock bitset computation. */

#include "fft/main.h"
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

    unlock_bit = UNIT_UNLOCKED_JOB_SQUIRE;
    unlocked = UNIT_UNLOCKED_JOB_SQUIRE;
    for (job = 0; job < UNIT_CAREER_JOB_COUNT - 1; job++) {
        unlock_bit /= 2;
        blocked = 0;
        for (j = 0; j < UNIT_JOB_LEVEL_BYTE_COUNT; j++) {
            if ((g_main_job_unlock_requirements[job][j] & UNIT_JOB_LEVEL_HIGH_NIBBLE_MASK)
                > (levels[j] & UNIT_JOB_LEVEL_HIGH_NIBBLE_MASK)) {
                blocked = 1;
                break;
            }
            if ((g_main_job_unlock_requirements[job][j] & UNIT_JOB_LEVEL_LOW_NIBBLE_MASK)
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
s32 main_job_add_proposition_jp(u32 party_index, s32 jp) {
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
    party = main_party_get_unit(party_index);
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
    level = main_job_calculate_level(total_jp);

    levels = party->career.job_levels[slot / 2];
    level_byte = level;
    if ((slot & 1) != 0) {
        levels = level + (levels & 0xF0);
    } else {
        levels = (levels & 0xF) | (level_byte << 4);
    }
    party->career.job_levels[slot / 2] = levels;

    main_job_store_unlock_bitset(
        party->career.unlocked_jobs, main_job_calculate_unlocked(party->career.job_levels, party->gender_flags));
    return 0;
}

/* Store the low 24 bits of unlocked_jobs as three bytes, most significant first. */
void main_job_store_unlock_bitset(u8 destination[UNIT_UNLOCKED_JOB_BYTE_COUNT], u32 unlocked_jobs) {
    *destination++ = unlocked_jobs >> 16;
    *destination++ = unlocked_jobs >> 8;
    *destination = unlocked_jobs;
}

/* Job level: how many of the eight thresholds in
 * g_main_job_level_jp_requirements the given JP has reached (0..8). */
u32 main_job_calculate_level(u16 job_jp) {
    s32 job_level = 0;
    s32 level;

    for (level = 0; level < UNIT_JOB_LEVEL_MAX; level++) {
        if (job_jp >= g_main_job_level_jp_requirements[level]) {
            job_level++;
        }
    }
    return job_level;
}
