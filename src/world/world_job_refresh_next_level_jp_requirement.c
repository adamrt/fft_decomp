#include "fft/job.h"
#include "fft/main_runtime.h"
#include "fft/world.h"
#include "psx/types.h"

/* WORLD twin of bunit_job_calculate_next_level_jp_requirement. */
s32 world_job_refresh_next_level_jp_requirement(s32 index) {
    s32 level;

    /* The definition's u16 parameter and s16 return conversions would change this call's codegen. */
    ((s32 (*)(s16))world_job_get_generic_index)(g_world_menu_learn_unlocked_jobs[index]);
    level = g_world_job_cached_level & 0xf;
    if (level >= 8) {
        /* job level 8 = mastered: no next-level requirement */
        g_world_job_cached_level_jp_requirement |= 0x20000000;
    } else {
        g_world_job_cached_level_jp_requirement = g_job_level_jp_requirements[level];
    }
    return g_world_job_cached_level_jp_requirement;
}
