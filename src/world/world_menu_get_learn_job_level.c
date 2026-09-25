#include "fft/world.h"

/* Job level of the selected unit in the Learn Abilities menu's `index`th
 * unlocked job; levels are packed two per byte (odd jobs in the low nibble). */
s32 world_menu_get_learn_job_level(s32 index) {
    s32 job_index;

    /* The definition's u16 parameter and s16 return conversions would change this call's codegen. */
    job_index = ((s32 (*)(s16))world_job_get_generic_index)(g_world_menu_learn_unlocked_jobs[index]);
    g_world_job_cached_level
        = g_world_formation_unit_pointers[g_world_formation_selected_unit_index]->job_levels[job_index >> 1];
    if (job_index & 1) {
        g_world_job_cached_level = g_world_job_cached_level & 0xF;
    } else {
        g_world_job_cached_level = g_world_job_cached_level >> 4;
    }
    return g_world_job_cached_level;
}
