#include "fft/event_jobstts.h"
#include "psx/types.h"

s32 jobstts_job_find_first_for_skillset(s32 skillset_id) {
    s32 job_id = 0;

    do {
        if (main_job_get_data_pointer(job_id)->skillset == skillset_id) {
            return job_id;
        }
        job_id++;
    } while (job_id < JOB_ID_COUNT);

    return -1;
}
