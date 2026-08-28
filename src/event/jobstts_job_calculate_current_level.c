#include "fft/jobstts.h"
#include "psx/types.h"

s32 jobstts_job_calculate_current_level(s32 index) {
    s32 generic_job = jobstts_job_get_generic_index(g_jobstts_job_ids[index]);
    jobstts_unit_job_data_t* unit = g_jobstts_unit_data[0];
    s32 job_level_index = generic_job >> 1;
    u8 packed_levels;

    packed_levels = unit->job_levels[job_level_index];

    g_jobstts_job_current_level = packed_levels;
    if (generic_job & 1) {
        g_jobstts_job_current_level &= 0xf;
    } else {
        g_jobstts_job_current_level >>= 4;
    }
    return g_jobstts_job_current_level;
}
