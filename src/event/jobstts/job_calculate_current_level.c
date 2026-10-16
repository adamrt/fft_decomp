/* The unit's level in a listed job, cached for row callbacks. */

#include "fft/event_jobstts.h"
#include "fft/job.h"
#include "psx/types.h"

/* Job level of the unit in the job list's `index`th job, also left in
 * g_job_list_job_current_level; levels are packed two per byte (odd jobs in
 * the low nibble). */
s32 job_list_calculate_level(s32 index) {
    s32 generic_job = job_get_generic_index(g_job_list_job_ids[index]);

    g_job_list_job_current_level = g_status_screen_unit_data[0]->career.job_levels[generic_job >> 1];
    if (generic_job & 1) {
        g_job_list_job_current_level &= 0xf;
    } else {
        g_job_list_job_current_level >>= 4;
    }
    return g_job_list_job_current_level;
}
