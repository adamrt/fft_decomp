#include "fft/world.h"
#include "psx/types.h"

void world_formation_start_job_wheel_close(void) {
    if (g_world_job_wheel_job_count < 2) {
        g_world_formation_current_menu = 0x15;
    } else {
        g_world_job_wheel_closing = 1;
    }
    g_world_job_wheel_close_frame = 0;
    g_world_job_wheel_unit_index = g_world_formation_selected_unit_index;
}
