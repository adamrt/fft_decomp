#include "fft/world.h"
#include "psx/types.h"

s32 world_job_get_cached_level(s32 unused_row) {
    return g_menu_job_current_level;
}
