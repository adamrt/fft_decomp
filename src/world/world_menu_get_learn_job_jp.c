#include "fft/data.h"
#include "fft/world.h"

/* JP the selected unit has in the Learn Abilities menu's `index`th unlocked job. */
u16 world_menu_get_learn_job_jp(s32 index) {
    s32 job_index;

    /* The definition's u16 parameter and s16 return conversions would change this call's codegen. */
    job_index = ((s32 (*)(s16))world_job_get_generic_index)(g_world_menu_learn_unlocked_jobs[index]);
    return g_world_formation_unit_pointers[g_world_formation_selected_unit_index]->job_points[job_index];
}
