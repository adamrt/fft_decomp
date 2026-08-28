#include "fft/data.h"
#include "fft/world.h"
#include "psx/types.h"

/* Job level of the job under the job-wheel cursor; job_levels packs two
 * generic jobs per byte, the even index in the high nibble. */
s32 world_formation_get_job_wheel_cursor_job_level(void) {
    /* The definition's u16 parameter and s16 return conversions would change this call's codegen. */
    s32 index
        = ((s32 (*)(s32))world_job_get_generic_index)(((s16*)g_world_job_wheel_jobs)[g_world_job_wheel_cursor_index]);
    s32 level = g_world_formation_unit_pointers[g_world_formation_selected_unit_index]->job_levels[index >> 1];

    if ((index & 1) == 0) {
        level >>= 4;
    } else {
        level &= 0xf;
    }
    return level;
}
