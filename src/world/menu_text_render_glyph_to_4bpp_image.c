/* Drawing one font glyph into a 4bpp image in a text style's palette entries. */

#include "fft/bits.h"
#include "fft/menu_text.h"
#include "fft/text.h"
#include "psx/types.h"

/* Decodes glyph `glyph` from the packed 2bpp font (14 rows of 10 pixels)
 * into a 4bpp image at `image`, at target x/y with a row stride of
 * row_stride 16-bit values. Each nonzero pixel adds `palette * 4`, picking one
 * four-color group of the 16-color CLUT; 0 stays transparent. Returns the
 * glyph's advance width.
 *
 * An odd x starts mid-byte: the first pixel fills the high nibble of the
 * existing byte, and the row's last byte is stored whole, clearing the pixel
 * to the right of the glyph. */
s32 menu_text_render_glyph_to_4bpp_image(
    s32 glyph, u8* image, const battle_text_image_position_t* target, s32 palette) {
    s32 row;
    s32 col;
    u8* dst;
    u8* font;
    u8 pixel;
    u8 byte;

    /* The font pointer goes through a local for the exact match. */
    font = g_text_glyph_bitmap_data;
    bits_init_primary_reader(font + glyph * TEXT_GLYPH_BYTES);
    row = 0;
    if ((target->x & 1) == 0) {
        for (; row < 14; row++) {
            dst = image + (target->row_stride << 1) * (target->y + row) + (target->x >> 1);
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
            dst = image + (target->row_stride << 1) * (target->y + row) + (target->x >> 1);
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
