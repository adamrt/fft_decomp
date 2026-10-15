#include "fft/main.h"
#include "fft/world.h"

/* Caches and returns the selected unit's total JP for the Learn Abilities
 * menu's `index`th unlocked job; the job list's menu script reads it as a
 * number callback. */
s32 world_job_calculate_current_total_jp(s32 index) {
    s32 job_index;

    job_index = job_get_generic_index(g_menu_job_ids[index]);
    return g_world_job_cached_total_jp
        = g_world_unit_pointers[g_unit_selected_index]->job_points.total.values[job_index];
}
