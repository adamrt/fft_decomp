#include "fft/main.h"

/* Store generic-job unlocks in the party record's big-endian byte order. */
void job_calculate_and_store_unlocks(party_unit_t* party, u8 unlocked_jobs[UNIT_UNLOCKED_JOB_BYTE_COUNT]) {
    s32 jobs = main_job_calculate_unlocked(
        party->career.job_levels, party->gender_flags & (UNIT_FLAG_FEMALE | UNIT_FLAG_MALE));
    unlocked_jobs[0] = jobs >> 16;
    unlocked_jobs[1] = jobs >> 8;
    unlocked_jobs[2] = jobs;
}
