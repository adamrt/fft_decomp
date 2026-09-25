#include "fft/world.h"
#include "psx/types.h"

s16 world_job_get_text_index(s16 job_id) {
    if (job_id < JOB_ID_SQUIRE) {
        return job_id + 19;
    }
    return job_id - JOB_ID_SQUIRE;
}
