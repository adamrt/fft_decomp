/* The lowest job id with a given skillset. */

#include "fft/job.h"
#include "psx/types.h"

/* Return the lowest job ID with this skillset, or -1. */
s32 job_find_first_for_skillset(s32 skillset) {
    s32 i;

    for (i = 0; i < JOB_ID_COUNT; i++) {
        if (job_get_data(i)->skillset == skillset) {
            return i;
        }
    }
    return -1;
}
