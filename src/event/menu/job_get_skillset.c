#include "fft/main.h"
#include "psx/types.h"

s32 job_get_skillset(s32 job_id) {
    return main_job_get_data_pointer(job_id)->skillset;
}
