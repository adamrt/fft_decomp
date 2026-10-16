#include "fft/job.h"
#include "psx/types.h"

/* Row callback: return whether the selected job is mastered; the row is ignored. */
s32 job_is_current_mastered(s32 unused_row) {
    return g_job_current_mastered;
}
