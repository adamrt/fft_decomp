/* Drawing a number right to left in large glyphs, with sign and marker glyphs. */

#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_text.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Draws `value` right to left as `digits & 0xff` large glyphs at `pos`, with
 * the sign and marker glyphs selected by the flag bits of `digits`. Sibling of
 * menu_draw_number_glyphs (0x8014ac30) on the 5-pixel font. WORLD links its
 * own body under this name: it lacks the leading 0x800 glyph and the hidden
 * value check, and draws every digit as 0xba under flag 0x800 instead. */
void text_draw_large_number_glyphs(s32 value, s32 digits, u8* pixels, glyph_blit_t* pos) {
    s32 i;
    s32 cursor;
    glyph_blit_t* src = &g_text_large_number_glyph_rect;

    cursor = digits & 0xFF00;
    if (cursor == 0xC00 || cursor == 0xE00) {
        g_text_large_number_glyph_rect.rect.y = 0x10;
        if (digits & 1) {
            src->rect.x = 0xB4;
            text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, src, pos);
            pos->rect.x += 5;
        }
        src->rect.x = 0xBA;
        if (cursor == 0xE00) {
            src->rect.x = 0xD0;
        }
        for (i = 0; i < 3; i++) {
            text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_text_large_number_glyph_rect, pos);
            pos->rect.x += 5;
        }
        return;
    }
    if (digits & 0x800) {
        src->rect.x = 0xBA;
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, src, pos);
        pos->rect.x += 7;
    }
    if (g_menu_hide_numeric_values == 0) {
        if ((digits & 0x200) && value == 0x64) {
            digits = cursor + 3;
        }
        if ((digits & 0x400) && value != 0x64) {
            digits = (digits & 0xFF00) + 2;
            pos->rect.x += 6;
        }
    }
    if (digits & 0x1000) {
        src->rect.x = 0xB4;
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_text_large_number_glyph_rect, pos);
        pos->rect.x += 5;
    }
    if (digits & 0x8000) {
        pos->rect.x -= 2;
        src->rect.x = 0xB4;
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_text_large_number_glyph_rect, pos);
        pos->rect.x += 7;
    }
    if (digits & 0x100) {
        src->rect.x = 0x78;
        src->rect.y = 0x1A;
        src->rect.h = 4;
        pos->rect.y += 3;
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_text_large_number_glyph_rect, pos);
        pos->rect.x += 7;
        pos->rect.y -= 3;
        src->rect.y = 0x10;
        src->rect.h = 0xA;
    }
    pos->rect.x += ((digits & 0xFF) - 1) * 5;
    cursor = pos->rect.x + 5;
    for (i = 0; i < (digits & 0xFF); i++) {
        src->rect.x = (value % 10) * 6 + 0x78;
        if (g_menu_hide_numeric_values != 0) {
            src->rect.x = 0xD0;
        }
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_text_large_number_glyph_rect, pos);
        value /= 10;
        pos->rect.x -= 5;
    }
    pos->rect.x = cursor;
    if (digits & 0x2000) {
        src->rect.x = 0xC0;
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_text_large_number_glyph_rect, pos);
        pos->rect.x += 5;
    }
}
