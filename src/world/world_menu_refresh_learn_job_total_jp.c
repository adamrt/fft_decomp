#include "fft/data.h"
#include "fft/world.h"

/* Caches the selected unit's total JP for the Learn Abilities menu's `index`th unlocked job. */
void world_menu_refresh_learn_job_total_jp(s32 index) {
    s32 job_index;

    /* The definition's u16 parameter and s16 return conversions would change this call's codegen. */
    job_index = ((s32 (*)(s16))world_job_get_generic_index)(g_world_menu_learn_unlocked_jobs[index]);
    g_world_job_cached_total_jp
        = g_world_formation_unit_pointers[g_world_formation_selected_unit_index]->total_job_points[job_index];
}
