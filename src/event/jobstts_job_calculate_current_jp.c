#include "fft/jobstts.h"
#include "psx/types.h"

void jobstts_job_calculate_current_jp(s32 index) {
    s32 generic_job = jobstts_job_get_generic_index(g_jobstts_job_ids[index]);

    g_jobstts_job_current_jp = g_jobstts_unit_data[0]->job_points[generic_job];
}
