#include "fft/main.h"
#include "fft/world.h"

/* Job level of the selected unit in the Learn Abilities menu's `index`th
 * unlocked job; levels are packed two per byte (odd jobs in the low nibble). */
s32 job_calculate_current_level_variant(s32 index) {
    s32 job_index;

    job_index = job_get_generic_index(g_menu_job_ids[index]);
    g_menu_job_current_level = g_world_unit_pointers[g_unit_selected_index]->job_levels[job_index >> 1];
    if (job_index & 1) {
        g_menu_job_current_level = g_menu_job_current_level & 0xF;
    } else {
        g_menu_job_current_level = g_menu_job_current_level >> 4;
    }
    return g_menu_job_current_level;
}
