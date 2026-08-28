#include "fft/jobstts.h"
#include "psx/types.h"

s32 jobstts_job_calculate_current_mastered(s32 index) {
    s16 ability_list[24];

    g_jobstts_job_current_mastered
        = jobstts_ability_build_list(0, g_jobstts_job_ids[index], JOBSTTS_ABILITY_CATEGORY_ALL, ability_list, 2) == 0;
    return g_jobstts_job_current_mastered;
}
