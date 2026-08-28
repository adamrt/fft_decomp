#include "fft/world.h"
#include "psx/types.h"

/* The screen-covering tile consumes g_world_screen_fade as RGB intensity, not alpha.
 * Yield once per step, then clamp to 255. */
void world_gfx_ramp_screen_overlay_intensity_to_max(void) {
    s32 level;

    g_world_screen_fade = 0;
    do {
        world_thread_yield();
        level = g_world_screen_fade + 4;
        g_world_screen_fade = level;
    } while (level < 256);
    g_world_screen_fade = 255;
}
