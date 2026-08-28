#include "fft/bunit.h"
#include "fft/job.h"
#include "fft/main_runtime.h"
#include "psx/types.h"

u32 bunit_job_calculate_next_level_jp_requirement(s32 index) {
    s32 level;

    bunit_job_get_generic_index(g_bunit_job_ids[index]);
    level = g_bunit_job_current_level & 0xf;
    if (level >= 8) {
        g_bunit_job_next_level_jp_requirement |= 0x20000000;
    } else {
        g_bunit_job_next_level_jp_requirement = g_job_level_jp_requirements[level];
    }
    return g_bunit_job_next_level_jp_requirement;
}
