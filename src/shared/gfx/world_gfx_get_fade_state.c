#include "fft/world.h"

/* Fade state: bit 0 while the fade toward zero intensity runs, bit 1 while
 * the fade toward full intensity runs; always none while fades are disabled. */
/* Target 0x8012d418. */
s32 world_gfx_get_fade_state(void) {
    if (g_world_gfx_fade_disabled != 0) {
        return WORLD_FADE_STATE_NONE;
    }
    return g_world_gfx_fade_decreasing + (g_world_gfx_fade_increasing * WORLD_FADE_STATE_INCREASING);
}
