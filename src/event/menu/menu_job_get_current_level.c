/* A row callback returning the selected job's level. */

#include "fft/job.h"
#include "psx/types.h"

/* Row callback: return the selected job's level; the row is ignored. */
s32 menu_job_get_current_level(s32 unused_row) {
    return g_menu_job_current_level;
}
