#include "fft/data.h"
#include "fft/world.h"

u16 world_job_get_selected_unit_job_points_entry(void) {
    return g_world_formation_unit_pointers[g_world_formation_selected_unit_index]
        ->job_points[g_world_learn_selected_job_index];
}
