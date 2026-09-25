#include "fft/event_card.h"
#include "psx/types.h"

s32 card_gfx_get_fade_state(void) {
    return g_card_gfx_decreasing_fade_active + (g_card_gfx_increasing_fade_active << 1);
}
