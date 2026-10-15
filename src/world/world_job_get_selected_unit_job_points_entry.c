#include "fft/world.h"

/* Menu callback: the selected formation unit's JP in the job chosen in the
 * learn menu. The row argument is not read. */
s32 world_job_get_selected_unit_job_points_entry(s32 unused_row) {
    return g_world_unit_pointers[g_unit_selected_index]->job_points.current.values[g_world_learn_selected_job_index];
}
