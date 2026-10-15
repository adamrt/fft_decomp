#include "fft/main.h"
#include "psx/types.h"

/* Return the first job id below JOB_ID_COUNT - 1 whose job record has this
 * skillset. The result is 0 when none has, which is also job 0's id. */
s32 main_job_find_by_skillset(u8 skillset_id) {
    u8 job_id;
    s32 result;

    job_id = 0;
    for (; (result = job_id < JOB_ID_COUNT - 1) != 0; job_id++) {
        if (g_main_job_data_pointer[job_id].skillset == skillset_id) {
            return job_id;
        }
    }
    return result;
}
