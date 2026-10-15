#include "fft/event_jobstts.h"
#include "fft/main.h"
#include "fft/menu.h"
#include "psx/types.h"

/* Job level of the unit in the job list's `index`th job, also left in
 * g_menu_job_current_level; levels are packed two per byte (odd jobs in
 * the low nibble). */
s32 jobstts_job_calculate_current_level(s32 index) {
    s32 generic_job = job_get_generic_index(g_menu_job_ids[index]);
    jobstts_unit_job_data_t* unit = g_jobstts_unit_data[0];
    s32 job_level_index = generic_job >> 1;
    u8 packed_levels;

    packed_levels = unit->career.job_levels[job_level_index];

    g_menu_job_current_level = packed_levels;
    if (generic_job & 1) {
        g_menu_job_current_level &= 0xf;
    } else {
        g_menu_job_current_level >>= 4;
    }
    return g_menu_job_current_level;
}
