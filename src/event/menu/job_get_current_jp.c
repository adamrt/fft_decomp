/* A row callback returning the selected job's current JP. */

#include "fft/job.h"
#include "psx/types.h"

/* Row callback: return the selected job's current JP; the row is ignored. */
s32 job_get_current_jp(s32 unused_row) {
    return g_job_current_jp;
}
