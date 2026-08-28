#include "fft/job.h"
#include "fft/main_unit.h"
#include "psx/types.h"

/* Awards proposition JP to a roster unit's current generic job (special jobs
 * use slot 0), recomputes the job level nibble and the unlocked-job bitset.
 * Returns -1 when the unit or JP amount is invalid. */
s32 main_job_add_proposition_jp(s32 party_index, s32 jp) {
    s32 slot;
    party_data_t* party;
    s32 job;
    s32 byte_index;
    s32 total;
    s32 level;
    s32 levels;
    u8 level_byte;
    party_data_t* record;

    if (jp < 0) {
        return -1;
    }
    if ((u32)party_index >= 20) {
        return -1;
    }
    party = main_party_get_data_pointer(party_index);
    if (party->party_id == PARTY_ID_NONE) {
        return -1;
    }
    if (party->gender_flags & UNIT_FLAG_MONSTER) {
        return -1;
    }
    job = party->job_id;
    if ((u32)(job - JOB_ID_SQUIRE) < JOB_ID_GENERIC_COUNT) {
        /* Zero-byte hint: the original recomputes job - 0x4a here instead
         * of reusing the range-check temporary. */
        __asm__("" : "=r"(job) : "0"(job));
        slot = job - JOB_ID_SQUIRE;
    } else {
        slot = 0;
    }

    /* Each serialized JP entry is a little-endian byte pair. The local base
     * and doubled index preserve the target's base-before-index addition. */
    record = party;
    byte_index = slot + slot;
    total = record->job_points[byte_index] + (record->job_points[byte_index + 1] << 8) + jp;
    if (total >= 10000) {
        total = 9999;
    }
    record->job_points[byte_index] = total;
    record->job_points[byte_index + 1] = total / 256;

    total = record->total_job_points[byte_index] + (record->total_job_points[byte_index + 1] << 8) + jp;
    if (total >= 10000) {
        total = 9999;
    }
    record->total_job_points[byte_index] = total;
    record->total_job_points[byte_index + 1] = total / 256;
    level = main_job_calculate_level(total & 0xFFFF);

    levels = party->job_levels[slot / 2];
    level_byte = level;
    if ((slot & 1) != 0) {
        levels = level + (levels & 0xF0);
    } else {
        levels = (levels & 0xF) | (level_byte << 4);
    }
    party->job_levels[slot / 2] = levels;

    main_job_store_unlock_bitset(
        party->unlocked_jobs, main_job_calculate_unlocked(party->job_levels, party->gender_flags));
    return 0;
}
