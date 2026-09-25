#include "fft/event_bunit.h"
#include "psx/types.h"

void bunit_gfx_start_decreasing_fade(void) {
    g_bunit_gfx_decreasing_fade_active = 1;
    g_bunit_gfx_fade_intensity = 0xf0;
}
