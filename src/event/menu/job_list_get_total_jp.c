/* A row callback returning the selected job's total JP earned. */

#include "fft/job.h"
#include "fft/job_list.h"
#include "psx/types.h"

/* Row callback: return the selected job's total JP earned; the row is ignored. */
s32 job_list_get_total_jp(s32 unused_row) {
    return g_job_current_total_jp;
}
