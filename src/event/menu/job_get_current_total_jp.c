#include "fft/job.h"
#include "psx/types.h"

/* Row callback: return the selected job's total JP earned; the row is ignored. */
s32 job_get_current_total_jp(s32 unused_row) {
    return g_job_current_total_jp;
}
