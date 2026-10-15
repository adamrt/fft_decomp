#include "fft/world.h"
#include "psx/types.h"

s32 world_job_get_cached_level_jp_requirement(s32 unused_row) {
    return g_menu_job_next_level_jp_requirement;
}
