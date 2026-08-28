#include "fft/job.h"
#include "fft/main_unit.h"

s32 world_job_find_by_skillset(s32 skillset) {
    s32 i;

    for (i = 0; i < JOB_ID_COUNT; i++) {
        if (main_job_get_data_pointer(i)->skillset == skillset) {
            return i;
        }
    }
    return -1;
}
