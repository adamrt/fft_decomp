#include "fft/event_card.h"
#include "psx/types.h"

/* Fade state: bit 0 while the fade-in runs, bit 1 while the fade-out runs. */
s32 card_gfx_get_fade_state(void) {
    return g_gfx_decreasing_fade_active + (g_gfx_increasing_fade_active << 1);
}
