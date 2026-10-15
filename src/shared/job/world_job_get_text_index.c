#include "fft/world.h"
#include "psx/types.h"

/* Index of a job's entry in the job description and requirement texts: the
 * generic jobs come first (Squire is 0), and the special jobs below Squire
 * follow from index 19 + job id. */
s32 world_job_get_text_index(s16 job_id) {
    if (job_id < JOB_ID_SQUIRE) {
        return (s16)(job_id + 19);
    }
    return (s16)(job_id - JOB_ID_SQUIRE);
}
