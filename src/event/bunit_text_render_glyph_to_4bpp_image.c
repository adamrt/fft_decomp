#include "fft/battle_text.h"
#include "fft/bunit.h"
#include "psx/types.h"

extern const u8* g_text_glyph_bitmap_data;

s32 bunit_text_render_glyph_to_4bpp_image(
    s32 glyph_id, u8* image, const bunit_text_image_position_t* position, s32 style) {
    s32 row;
    const u8* glyph_bitmap;
    s32 col;
    s32 stride;
    u8* dst;
    u8 pixel;
    u8 byte;

    glyph_bitmap = g_text_glyph_bitmap_data;
    bunit_bits_init_reader(glyph_bitmap + glyph_id * 35);
    for (row = 0; row < 14; row++) {
        stride = position->row_stride * 2;
        dst = image + stride * (position->y + row) + ((s16)position->x >> 1);
        for (col = 0; col < 5; col++) {
            pixel = bunit_bits_read(2);
            if (pixel != 0) {
                pixel += style * 4;
            }
            byte = pixel;
            pixel = bunit_bits_read(2);
            if (pixel != 0) {
                pixel += style * 4;
            }
            byte |= pixel << 4;
            *dst++ = byte;
        }
    }
    return g_text_glyph_widths[glyph_id];
}
