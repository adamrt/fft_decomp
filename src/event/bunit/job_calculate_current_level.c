/* The selected unit's level in a listed job, cached for row callbacks. */

#include "fft/event_bunit.h"
#include "fft/job.h"
#include "fft/unit_party.h"
#include "psx/types.h"

/* Job level of the selected unit in the job list's `index`th job, also left
 * in g_menu_job_current_level; levels are packed two per byte (odd jobs in
 * the low nibble). */
s32 bunit_job_calculate_current_level(s32 index) {
    s32 generic_job = job_get_generic_index(g_menu_job_ids[index]);

    g_menu_job_current_level = g_bunit_unit_pointers[g_unit_selected_index]->job_levels[generic_job >> 1];
    if ((generic_job & 1) != 0) {
        g_menu_job_current_level &= 0xf;
    } else {
        g_menu_job_current_level >>= 4;
    }
    return g_menu_job_current_level;
}
