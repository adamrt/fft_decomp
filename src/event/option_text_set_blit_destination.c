#include "fft/event_option.h"
#include "psx/types.h"

void option_text_set_blit_destination(s16 destination_x, s16 destination_y) {
    g_option_text_glyph_dest.rect.x = destination_x;
    g_option_text_glyph_dest.rect.y = destination_y;
}
