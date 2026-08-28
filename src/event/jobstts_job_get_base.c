#include "fft/job.h"
#include "fft/jobstts.h"
#include "psx/types.h"

s32 jobstts_job_get_base(s16 unit_id) {
    jobstts_unit_job_data_t* unit = g_jobstts_unit_data[unit_id];
    s32 job_id = unit->character_identity;

    if (job_id == CHARACTER_IDENTITY_MONSTER) {
        job_id = unit->job_id;
    } else if ((u32)job_id >= CHARACTER_IDENTITY_GENERIC_MALE) {
        job_id = JOB_ID_SQUIRE;
    }
    return job_id;
}
