#include "fft/main.h"
#include "psx/types.h"

enum {
    RANDOM_JOB_COUNT = JOB_ID_MIME - JOB_ID_GENERIC_FIRST,
};

/* Return a random unlocked generic job ID below Mime, or 0 when none are unlocked.
 *
 * The ENTD caller explicitly normalizes the zero-extended job byte. */
s32 main_job_get_random_unlocked(const unit_stats_t* unit) {
    u8 candidates[RANDOM_JOB_COUNT];
    s32 job_mask = UNIT_UNLOCKED_JOB_SQUIRE;
    s32 candidate_count = 0;
    s32 job_index;
    s32 unlocked_jobs
        = (unit->career.unlocked_jobs[0] << 16) + (unit->career.unlocked_jobs[1] << 8) + unit->career.unlocked_jobs[2];

    for (job_index = 0; job_index < RANDOM_JOB_COUNT; job_index++, job_mask /= 2) {
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
    }

    if (candidate_count != 0) {
        s32 scaled_random = rand() * candidate_count;
        u8 candidate_index = (u32)scaled_random >> 15;

        if (scaled_random < 0) {
            candidate_index = (u32)(scaled_random + 0x7fff) >> 15;
        }
        return candidates[candidate_index];
    }
    return 0;
}
