#include "fft/character_identity.h"
#include "fft/job.h"
#include "fft/main_unit.h"
#include "psx/types.h"

#define JOB_COUNT 19

/* Return a random unlocked generic job ID, or 0 when none are unlocked.
 *
 * The ENTD caller explicitly normalizes the zero-extended job byte. */
s32 main_job_get_random_unlocked(const battle_stats_t* unit) {
    u8 candidates[JOB_COUNT];
    s32 job_mask = 0x800000;
    s32 candidate_count = 0;
    s32 job_index = 0;
    s32 unlocked_jobs = (unit->unlocked_jobs[0] << 16) + (unit->unlocked_jobs[1] << 8) + unit->unlocked_jobs[2];

    do {
        if (unlocked_jobs & job_mask) {
            u8 job_id = job_index + JOB_ID_SQUIRE;

            if (job_index == 0) {
                job_id = unit->character_identity;
                if (job_id >= CHARACTER_IDENTITY_SELECTOR_FIRST) {
                    job_id = JOB_ID_SQUIRE;
                }
            }
            candidates[candidate_count] = job_id;
            candidate_count++;
        }
        job_index++;
        job_mask /= 2;
    } while (job_index < JOB_COUNT);

    if (candidate_count != 0) {
        s32 scaled_random = rand() * candidate_count;
        u32 candidate_index = (u32)scaled_random >> 15;

        if (scaled_random < 0) {
            candidate_index = (u32)(scaled_random + 0x7fff) >> 15;
        }
        return candidates[candidate_index & 0xff];
    }
    return 0;
}
