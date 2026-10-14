#include "fft/gfx.h"
#include "fft/thread.h"

/* Fade the screen back in: lower g_gfx_screen_fade from 255 by 4 per frame,
 * yielding once per step, and end on 0. */
void gfx_ramp_screen_overlay_intensity_to_zero(void) {
    g_gfx_screen_fade = 0xFF;
    do {
        thread_yield();
        g_gfx_screen_fade -= 4;
    } while (g_gfx_screen_fade > 0);
    g_gfx_screen_fade = 0;
}
