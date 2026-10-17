/* A job's skillset from the job data table. */

#include "fft/job.h"
#include "psx/types.h"

/* Return the skillset of a job from the job data table. */
s32 job_get_skillset(s32 job_id) {
    return job_get_data(job_id)->skillset;
}
