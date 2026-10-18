/* Setting the source rectangle of the image blitter. */

#include "fft/screen.h"
#include "psx/types.h"

extern glyph_blit_t g_screen_image_blit_source;

/* Set the source rectangle used by the WORLD image blitter. */
void screen_set_blit_source_rect(s16 x, s16 y, s16 width, s16 height) {
    g_screen_image_blit_source.rect.x = x;
    g_screen_image_blit_source.rect.y = y;
    g_screen_image_blit_source.rect.w = width;
    g_screen_image_blit_source.rect.h = height;
}
