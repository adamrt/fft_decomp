#include "fft/card.h"
#include "psx/types.h"

void card_gfx_set_draw_color(const u8* data) {
    g_card_text_color[0] = data[0];
    g_card_text_color[1] = data[1];
    g_card_text_color[2] = data[2];
}
