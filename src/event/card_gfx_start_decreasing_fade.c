#include "fft/event_card.h"

/* Start the fade from intensity 0xf0 down to zero (the screen fades in). */
void card_gfx_start_decreasing_fade(void) {
    g_gfx_decreasing_fade_active = 1;
    g_gfx_fade_intensity = 0xf0;
}
