#include "fft/world.h"

/* Target 0x8012d48c. */
void world_gfx_start_increasing_fade(void) {
    if (g_world_gfx_fade_disabled == 0) {
        g_world_gfx_fade_decreasing = 0;
        g_world_gfx_fade_increasing = 1;
        g_world_gfx_fade_intensity = 0;
    }
}
