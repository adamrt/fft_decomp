#include "fft/jobstts.h"
#include "psx/types.h"

s32 jobstts_job_is_current_mastered(void) {
    return g_jobstts_job_current_mastered;
}
