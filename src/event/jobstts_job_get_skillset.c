#include "fft/main_unit.h"
#include "psx/types.h"

s32 jobstts_job_get_skillset(s32 job_id) {
    return main_job_get_data_pointer(job_id)->skillset;
}
