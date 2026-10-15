#include "fft/event_bunit.h"
#include "psx/types.h"

/* Fade state: bit 0 while the fade-in runs, bit 1 while the fade-out runs. */
s32 bunit_gfx_get_fade_state(void) {
    s32 second;
    s32 first;

    second = g_gfx_increasing_fade_active;
    first = g_gfx_decreasing_fade_active;
    second *= 2;
    return first + second;
}
