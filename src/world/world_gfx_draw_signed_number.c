#include "fft/world.h"
#include "psx/types.h"

/*
 * Draw a signed decimal number right-aligned in `digits` columns of 6 pixels
 * starting at the pen, least significant digit first, with a minus glyph drawn
 * before the digits when the value is negative. Glyphs are 0x23 bytes apart
 * in the font block at g_world_text_glyph_bitmap_data.
 *
 * `digits` arrives as a full 32-bit value and only its low byte is used.
 */
void world_gfx_draw_signed_number(s32 value, s32 digits, u8* image, glyph_blit_t* pen) {
    s32 original;
    s32 i;

    original = value;
    pen->rect.x += ((u8)digits - 1) * 6;
    if (original < 0) {
        value = -value;
        pen->rect.x += 6;
    }
    for (i = 0; i < (u8)digits; i++) {
        text_render_glyph_to_4bpp_image(g_text_glyph_bitmap_data + (value % 10) * TEXT_GLYPH_BYTES, image, pen, 0);
        pen->rect.x -= 6;
        value /= 10;
    }
    if (original < 0) {
        text_render_glyph_to_4bpp_image(g_text_glyph_bitmap_data + 0x2067, image, pen, 0);
    }
}
