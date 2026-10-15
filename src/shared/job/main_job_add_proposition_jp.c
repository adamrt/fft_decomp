#include "fft/main.h"
#include "psx/types.h"

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
