/* Setting where the next glyph blit lands in the text image. */

#include "fft/text.h"
#include "psx/types.h"

/* Set where the next glyph blit lands in the text image. */
void text_set_blit_destination(s16 destination_x, s16 destination_y) {
    g_text_glyph_dest.rect.x = destination_x;
    g_text_glyph_dest.rect.y = destination_y;
}
