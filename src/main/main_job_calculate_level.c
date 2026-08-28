#include "fft/job.h"
#include "fft/main_runtime.h"
#include "psx/types.h"

u32 main_job_calculate_level(u16 job_jp) {
    s32 job_level = 0;
    s32 level;

    for (level = 0; level < 8; level++) {
        if (job_jp >= g_job_level_jp_requirements[level]) {
            job_level++;
        }
    }
    return job_level;
}
