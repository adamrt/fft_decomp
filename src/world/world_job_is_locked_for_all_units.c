#include "fft/world.h"

s32 world_job_is_locked_for_all_units(s16 job_id) {
    s32 i;

    for (i = 0; i < g_world_formation_unit_count; i++) {
        if (world_job_is_special_monster(g_world_formation_unit_pointers[i]->job_id) == 0) {
            world_bit_cursor_set_secondary(g_world_formation_unit_pointers[i]->unlocked_jobs);
            world_bit_cursor_read_secondary(job_id - JOB_ID_SQUIRE);
            if (world_bit_cursor_read_secondary(1) != 0) {
                return 0;
            }
        }
    }
    return 1;
}
