/* A row callback returning the selected job's level. */

#include "fft/job_list.h"
#include "psx/types.h"

/* Row callback: return the selected job's level; the row is ignored. */
s32 job_list_get_level(s32 unused_row) {
    return g_job_list_job_current_level;
}
