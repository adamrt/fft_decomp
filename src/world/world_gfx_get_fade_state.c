#include "fft/world.h"

/* Target 0x8012d418. */
s32 world_gfx_get_fade_state(void) {
    if (g_world_gfx_fade_disabled != 0) {
        return WORLD_FADE_STATE_NONE;
    }
    return g_world_gfx_fade_decreasing + (g_world_gfx_fade_increasing * WORLD_FADE_STATE_INCREASING);
}
