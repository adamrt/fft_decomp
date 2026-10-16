/* The selected unit's current JP in a listed job, cached for row callbacks. */

#include "fft/job.h"
#include "fft/job_list.h"
#include "fft/unit.h"
#include "fft/unit_list.h"
#include "psx/types.h"

/* Also returns the value: callers read it through g_menu_row_callbacks. */
s32 job_list_calculate_jp(s32 index) {
    s32 generic_job = job_get_generic_index(g_job_list_job_ids[index]);

    return g_job_current_jp = g_unit_list_pointers[g_unit_selected_index]->job_points.current.values[generic_job];
}
