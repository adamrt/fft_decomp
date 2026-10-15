#include "fft/world.h"

/* Begin the job wheel's close animation; with fewer than two jobs there is
 * no wheel and WORLD_FORMATION_MENU_JOB_WHEEL_OPENING is set at once. */
void world_formation_start_job_wheel_close(void) {
    if (g_world_job_wheel_job_count < 2) {
        g_world_formation_current_menu = WORLD_FORMATION_MENU_JOB_WHEEL_OPENING;
    } else {
        g_world_job_wheel_closing = 1;
    }
    g_world_job_wheel_close_frame = 0;
    g_world_job_wheel_unit_index = g_unit_selected_index;
}
