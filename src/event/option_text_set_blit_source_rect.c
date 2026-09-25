#include "fft/event_option.h"
#include "psx/types.h"

void option_text_set_blit_source_rect(s16 source_x, s16 source_y, s16 width, s16 height) {
    g_option_text_glyph_source.rect.x = source_x;
    g_option_text_glyph_source.rect.y = source_y;
    g_option_text_glyph_source.rect.w = width;
    g_option_text_glyph_source.rect.h = height;
}
