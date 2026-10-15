#include "fft/world.h"

/* Start the fade from intensity 240 down to zero (the screen fades in),
 * unless fades are disabled. */
/* Target 0x8012d454. */
void world_gfx_start_decreasing_fade(void) {
    if (g_world_gfx_fade_disabled == 0) {
        g_world_gfx_fade_decreasing = 1;
        g_world_gfx_fade_increasing = 0;
        g_world_gfx_fade_intensity = 240;
    }
}
