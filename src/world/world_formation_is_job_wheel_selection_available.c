#include "fft/world.h"
#include "psx/types.h"

s32 world_formation_is_job_wheel_selection_available(void) {
    if (g_world_job_wheel_job_count < 2) {
        return 0;
    }
    return ((g_world_job_wheel_jobs[g_world_job_wheel_cursor_index] >> 14) ^ 1) & 1;
}
