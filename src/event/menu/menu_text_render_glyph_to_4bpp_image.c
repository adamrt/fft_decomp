/* Drawing one font glyph into a 4bpp image in a text style's palette entries. */

#include "fft/bits.h"
#include "fft/menu.h"
#include "fft/menu_text.h"
#include "fft/text.h"
#include "psx/types.h"

/* Draw one 10x14 font glyph into a 4bpp image at `position` and return the
 * glyph's width.
 *
 * A glyph is 35 bytes of 2-bit pixels, most significant bits first. A
 * nonzero pixel is stored as its value plus style * 4, which selects the
 * style's four palette entries; an odd x is rounded down to the even pixel. */
s32 menu_text_render_glyph_to_4bpp_image(
    s32 glyph_id, u8* image, const battle_text_image_position_t* position, s32 style) {
    s32 row;
    const u8* glyph_bitmap;
    s32 col;
    s32 stride;
    u8* dst;
    u8 pixel;
    u8 byte;

    glyph_bitmap = g_text_glyph_bitmap_data;
    bits_init_primary_reader(glyph_bitmap + glyph_id * TEXT_GLYPH_BYTES);
    for (row = 0; row < 14; row++) {
        stride = position->row_stride * 2;
        dst = image + stride * (position->y + row) + (position->x >> 1);
        for (col = 0; col < 5; col++) {
            pixel = bits_read_primary(2);
            if (pixel != 0) {
                pixel += style * 4;
            }
            byte = pixel;
            pixel = bits_read_primary(2);
            if (pixel != 0) {
                pixel += style * 4;
            }
            byte |= pixel << 4;
            *dst++ = byte;
        }
    }
    return g_text_glyph_widths[glyph_id];
}
