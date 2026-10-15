#include "fft/world.h"
#include "psx/types.h"

/* Decodes glyph `glyph` from the packed 2bpp font into a 4bpp image at
 * `image`, using target x/y and a row stride of w 16-bit values, adding `palette * 4`
 * to every non-zero pixel. Returns the glyph's advance width. */
s32 world_text_decode_font_glyph_to_4bpp(s32 glyph, u8* image, RECT* target, s32 palette) {
    s32 row;
    s32 col;
    u8* dst;
    u8* font;
    u8 pixel;
    u8 byte;

    font = g_text_glyph_bitmap_data;
    world_bit_cursor_set_primary(font + glyph * TEXT_GLYPH_BYTES);
    row = 0;
    if ((target->x & 1) == 0) {
        for (; row < 14; row++) {
            dst = image + (target->w << 1) * (target->y + row) + (target->x >> 1);
            for (col = 0; col < 5; col++) {
                pixel = bits_read_primary(2);
                if (pixel != 0) {
                    pixel += palette * 4;
                }
                byte = pixel;
                pixel = bits_read_primary(2);
                if (pixel != 0) {
                    pixel += palette * 4;
                }
                byte |= pixel << 4;
                *dst++ = byte;
            }
        }
    } else {
        for (; row < 14; row++) {
            dst = image + (target->w << 1) * (target->y + row) + (target->x >> 1);
            pixel = bits_read_primary(2);
            if (pixel != 0) {
                pixel += palette * 4;
            }
            byte = (*dst & 0xF) | (pixel << 4);
            *dst++ = byte;
            for (col = 0; col < 4; col++) {
                pixel = bits_read_primary(2);
                if (pixel != 0) {
                    pixel += palette * 4;
                }
                byte = pixel;
                pixel = bits_read_primary(2);
                if (pixel != 0) {
                    pixel += palette * 4;
                }
                byte |= pixel << 4;
                *dst++ = byte;
            }
            pixel = bits_read_primary(2);
            if (pixel != 0) {
                pixel += palette * 4;
            }
            *dst = pixel;
        }
    }
    return g_text_glyph_widths[glyph];
}
