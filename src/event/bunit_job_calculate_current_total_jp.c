#include "fft/bunit.h"
#include "psx/types.h"

void bunit_job_calculate_current_total_jp(s32 index) {
    s32 generic_job;
    bunit_unit_data_t* unit;

    generic_job = bunit_job_get_generic_index(g_bunit_job_ids[index]);
    unit = g_bunit_unit_data[g_bunit_unit_selected_index];
    g_bunit_job_current_total_jp = unit->total_job_points[generic_job];
}
