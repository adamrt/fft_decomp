#include "fft/world.h"
#include "psx/types.h"

/*
 * Draw a signed decimal number right-aligned in `digits` columns of 6 pixels
 * starting at *x, least significant digit first, with a minus glyph drawn
 * before the digits when the value is negative. Glyphs are 0x23 bytes apart
 * in the font block at g_world_text_glyph_bitmap_data.
 */
void world_gfx_draw_signed_number(s32 value, u8 digits, void* image, u16* x) {
    s32 original;
    s32 i;

    original = value;
    /* Extra uses of digits and image order the parameter copies and give digits $s4. */
    __asm__("" : : "r"(digits));
    *x += (digits - 1) * 6;
    __asm__("" : : "r"(image));
    if (original < 0) {
        value = -value;
        *x += 6;
    }
    for (i = 0; i < digits; i++) {
        world_text_blit_font_glyph_to_4bpp(g_world_text_glyph_bitmap_data + (value % 10) * 0x23, image, x, 0);
        *x -= 6;
        value /= 10;
    }
    if (original < 0) {
        world_text_blit_font_glyph_to_4bpp(g_world_text_glyph_bitmap_data + 0x2067, image, x, 0);
    }
}
