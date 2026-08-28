#include "fft/card.h"
#include "psx/types.h"

s32 card_text_render_glyph_to_4bpp_image(
    s32 glyph_id, u8* image, const card_text_image_position_t* position, s32 style) {
    s32 row;
    const u8* glyph_bitmap;
    s32 col;
    s32 stride;
    u8* dst;
    u8 pixel;
    u8 byte;

    glyph_bitmap = g_card_text_glyph_bitmap_data;
    card_bits_init_primary_reader(glyph_bitmap + glyph_id * 35);
    for (row = 0; row < 14; row++) {
        stride = position->row_stride * 2;
        dst = image + stride * (position->y + row) + ((s16)position->x >> 1);
        for (col = 0; col < 5; col++) {
            pixel = card_bits_read_primary(2);
            if (pixel != 0) {
                pixel += style * 4;
            }
            byte = pixel;
            pixel = card_bits_read_primary(2);
            if (pixel != 0) {
                pixel += style * 4;
            }
            byte |= pixel << 4;
            *dst++ = byte;
        }
    }
    return g_card_text_glyph_widths[glyph_id];
}
