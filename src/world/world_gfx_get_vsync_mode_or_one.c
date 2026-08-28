#include "fft/world.h"

s32 world_gfx_get_vsync_mode_or_one(void) {
    s32 value = g_world_gfx_vsync_mode;
    if (value == 0) {
        return 1;
    }
    return value;
}
