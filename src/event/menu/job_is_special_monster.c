/* Whether a job id is in one of the special monster ranges. */

#include "fft/data_job.h"
#include "fft/job.h"
#include "psx/types.h"

/* Returns 1 for a job id in either special-monster range: from
 * JOB_ID_SPECIAL_MONSTER_1_FIRST up to the first generic job, or the
 * block that starts at JOB_ID_SPECIAL_MONSTER_2_FIRST. */
s32 job_is_special_monster(s32 job_id) {
    s32 result = job_id >= JOB_ID_SPECIAL_MONSTER_1_FIRST && job_id < JOB_ID_GENERIC_FIRST;

    if (job_id >= JOB_ID_SPECIAL_MONSTER_2_FIRST
        && job_id < JOB_ID_SPECIAL_MONSTER_2_FIRST + JOB_ID_SPECIAL_MONSTER_2_COUNT) {
        result = 1;
    }
    return result;
}
