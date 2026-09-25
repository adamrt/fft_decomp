#include "fft/event_bunit.h"
#include "psx/types.h"

s32 bunit_job_calculate_current_level(s32 index) {
    s32 generic_job;
    bunit_unit_data_t* unit;
    s32 job_level_index;
    u8 packed_levels;

    generic_job = bunit_job_get_generic_index(g_bunit_job_ids[index]);
    unit = g_bunit_unit_data[g_bunit_unit_selected_index];
    job_level_index = generic_job >> 1;
    packed_levels = unit->job_levels[job_level_index];

    g_bunit_job_current_level = packed_levels;
    if ((generic_job & 1) != 0) {
        g_bunit_job_current_level &= 0xf;
    } else {
        g_bunit_job_current_level >>= 4;
    }
    return g_bunit_job_current_level;
}
