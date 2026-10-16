/* A job's skillset from the job data table. */

#include "fft/main.h"
#include "fft/main_unit.h"
#include "psx/types.h"

/* Return the skillset of a job from the job data table. */
s32 job_get_skillset(s32 job_id) {
    return main_job_get_data_pointer(job_id)->skillset;
}
