/* A row callback returning the JP the selected job needs for its next level. */

#include "fft/job_list.h"
#include "psx/types.h"

/* Row callback: return the JP the selected job needs for its next level; the row is ignored. */
s32 job_list_get_next_level_jp(s32 unused_row) {
    return g_job_list_job_next_level_jp_requirement;
}
