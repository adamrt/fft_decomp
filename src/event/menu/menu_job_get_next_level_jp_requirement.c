/* A row callback returning the JP the selected job needs for its next level. */

#include "fft/job.h"
#include "psx/types.h"

/* Row callback: return the JP the selected job needs for its next level; the row is ignored. */
s32 menu_job_get_next_level_jp_requirement(s32 unused_row) {
    return g_menu_job_next_level_jp_requirement;
}
