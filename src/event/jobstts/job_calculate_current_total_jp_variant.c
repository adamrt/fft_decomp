#include "fft/event_jobstts.h"
#include "fft/main.h"
#include "fft/menu_panel.h"
#include "psx/types.h"

/* Also returns the value: callers read it through g_menu_row_callbacks. */
s32 job_calculate_current_total_jp_variant(s32 index) {
    s32 generic_job = job_get_generic_index(g_menu_job_ids[index]);

    return g_job_current_total_jp = g_jobstts_unit_data[0]->career.job_points.total.values[generic_job];
}
