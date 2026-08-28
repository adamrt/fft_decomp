#include "fft/card.h"
#include "psx/types.h"

void card_gfx_set_render_otag(u32* otag) {
    g_card_gfx_render_otag = otag;
}
