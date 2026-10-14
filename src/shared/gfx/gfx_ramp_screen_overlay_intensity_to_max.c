#include "fft/gfx.h"
#include "fft/thread.h"

/* Fade the screen out: raise g_gfx_screen_fade from 0 by 4 per frame,
 * yielding once per step, and end on 255. */
void gfx_ramp_screen_overlay_intensity_to_max(void) {
    g_gfx_screen_fade = 0;
    do {
        thread_yield();
        g_gfx_screen_fade += 4;
    } while (g_gfx_screen_fade < 0x100);
    g_gfx_screen_fade = 0xff;
}
