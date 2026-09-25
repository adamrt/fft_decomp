#include "fft/event_jobstts.h"
#include "psx/types.h"

s32 jobstts_job_get_generic_index(s32 job_id) {
    u16 job_index = job_id - (unsigned long long)JOB_ID_SQUIRE;
    s16 result;

    if (job_index < JOB_ID_GENERIC_COUNT) {
        result = job_id - JOB_ID_SQUIRE;
    } else {
        result = 0;
    }
    return result;
}
