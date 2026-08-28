#include "fft/main_unit.h"
#include "fft/world.h"
#include "psx/types.h"

/* Calculate the roster unit's generic-job unlock mask and store its three
 * bytes in the big-endian order used by party and formation records. */
void world_job_calculate_and_store_unlocks(party_data_t* party, u8 unlocked_jobs[3]) {
    s32 jobs
        = main_job_calculate_unlocked(party->job_levels, party->gender_flags & (UNIT_FLAG_FEMALE | UNIT_FLAG_MALE));
    unlocked_jobs[0] = (s8)(jobs >> 16);
    unlocked_jobs[1] = (s8)(jobs >> 8);
    unlocked_jobs[2] = (s8)jobs;
}
