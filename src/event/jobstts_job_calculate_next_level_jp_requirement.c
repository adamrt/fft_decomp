#include "fft/event_jobstts.h"
#include "psx/types.h"

s32 jobstts_job_calculate_next_level_jp_requirement(s32 index) {
    s32 level;

    jobstts_job_get_generic_index(g_jobstts_job_ids[index]);
    level = g_jobstts_job_current_level & 0xf;
    if (level >= 8) {
        g_jobstts_job_next_level_jp_requirement |= 0x20000000;
    } else {
        g_jobstts_job_next_level_jp_requirement = g_job_level_jp_requirements[level];
    }
    return g_jobstts_job_next_level_jp_requirement;
}
