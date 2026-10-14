#include "fft/battle_text.h"
#include "fft/menu.h"

/* Draw the two stacked glyphs and leave the pen at the shifted lower position. */
void menu_draw_stacked_glyph_pair(u8* pixels, glyph_blit_t* position) {
    position->rect.x -= 2;
    position->rect.y += 2;
    text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_menu_stacked_glyph_upper, position);
    position->rect.y += 4;
    text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_menu_stacked_glyph_lower, position);
}
