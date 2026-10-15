#include "fft/main.h"
#include "psx/types.h"

/* Job level: how many of the eight thresholds in
 * g_main_job_level_jp_requirements the given JP has reached (0..8). */
u32 main_job_calculate_level(u16 job_jp) {
    s32 job_level = 0;
    s32 level;

    for (level = 0; level < UNIT_JOB_LEVEL_MAX; level++) {
        if (job_jp >= g_main_job_level_jp_requirements[level]) {
            job_level++;
        }
    }
    return job_level;
}
