#include "fft/battle_text.h"
#include "fft/menu.h"

/* Draw decimal glyphs right to left and clear the text palette afterward. */
void menu_display_number_entry(s32 value, u8 digit_count, u8* image, glyph_blit_t* origin) {
    s16 cell;

    origin->rect.x += (digit_count - 1) * 6;
    do {
        text_render_glyph_to_4bpp_image(
            g_text_glyph_bitmap_data + (value % 10) * TEXT_GLYPH_BYTES, image, origin, g_menu_text_palette_offset);
        value = value / 10;
        cell = origin->rect.x;
        origin->rect.x = cell - 6;
    } while (value != 0);
    g_menu_text_palette_offset = 0;
}
