/* A row callback returning the selected job's current JP. */

#include "fft/job_list.h"
#include "psx/types.h"

/* Row callback: return the selected job's current JP; the row is ignored. */
s32 job_list_get_jp(s32 unused_row) {
    return g_job_current_jp;
}
