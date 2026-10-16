#include "fft/text.h"
#include "psx/types.h"

/* Set the glyph-sheet rectangle the next glyph blit copies from. */
void text_set_blit_source_rect(s16 source_x, s16 source_y, s16 width, s16 height) {
    g_text_glyph_source.rect.x = source_x;
    g_text_glyph_source.rect.y = source_y;
    g_text_glyph_source.rect.w = width;
    g_text_glyph_source.rect.h = height;
}
