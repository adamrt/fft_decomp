#include "fft/world.h"
#include "psx/types.h"

s32 world_job_get_cached_skillset_empty_flag(s32 unused_row) {
    return g_job_current_mastered;
}
