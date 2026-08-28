#include "fft/job.h"
#include "psx/types.h"

s32 wldcore_map_job_id_to_category_index(s32 job_id) {
    if (job_id < JOB_ID_SQUIRE) {
        return 22;
    }
    if (job_id < JOB_ID_MIME + 1) {
        return job_id - JOB_ID_SQUIRE;
    }
    return 21;
}
