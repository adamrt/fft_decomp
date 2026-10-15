#include "fft/main.h"
#include "fft/world.h"

/* JP the selected unit has in the Learn Abilities menu's `index`th unlocked job. */
s32 world_menu_get_learn_job_jp(s32 index) {
    s32 job_index;

    job_index = job_get_generic_index(g_menu_job_ids[index]);
    return g_world_unit_pointers[g_unit_selected_index]->job_points.current.values[job_index];
}
