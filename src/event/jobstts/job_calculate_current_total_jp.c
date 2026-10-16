/* The unit's total JP earned in a listed job, cached for row callbacks. */

#include "fft/event_jobstts.h"
#include "fft/job.h"
#include "psx/types.h"

/* Also returns the value: callers read it through g_menu_row_callbacks. */
s32 job_list_calculate_total_jp(s32 index) {
    s32 generic_job = job_get_generic_index(g_job_list_job_ids[index]);

    return g_job_current_total_jp = g_status_screen_unit_data[0]->career.job_points.total.values[generic_job];
}
