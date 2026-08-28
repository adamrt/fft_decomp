#include "fft/card.h"
#include "psx/types.h"

void card_gfx_start_decreasing_fade(void) {
    g_card_gfx_decreasing_fade_active = 1;
    g_card_gfx_fade_intensity = 0xf0;
}
