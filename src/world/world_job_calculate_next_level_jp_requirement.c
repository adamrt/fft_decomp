#include "fft/main.h"
#include "fft/world.h"
#include "psx/types.h"

/* JP needed for the next level of the job whose level
 * world_job_calculate_current_level last cached: the table entry for levels
 * below 8, otherwise the previous value with the dashes flag set. The
 * `index`th job is looked up but its result is not used. */
s32 world_job_calculate_next_level_jp_requirement(s32 index) {
    s32 level;

    job_get_generic_index(g_menu_job_ids[index]);
    level = g_world_job_cached_level & 0xf;
    if (level >= UNIT_JOB_LEVEL_MAX) {
        /* The maximum job level has no next-level requirement. */
        g_world_job_cached_level_jp_requirement |= MENU_NUMBER_VALUE_DASHES;
    } else {
        g_world_job_cached_level_jp_requirement = g_main_job_level_jp_requirements[level];
    }
    return g_world_job_cached_level_jp_requirement;
}
