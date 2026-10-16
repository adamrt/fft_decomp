/* Drawing a number right to left in the small 6x10 number glyphs, with sign and prefix glyphs. */

#include "fft/gfx.h"
#include "fft/menu_gfx.h"
#include "fft/text.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/*
 * Render a decimal value into a text image, right to left, as `format & 0xff`
 * digit glyphs. The high byte of `format` adds glyphs, named here by their x
 * in the menu glyph sheet (the digits sit 6 pixels apart from x 0x78 and are
 * drawn 5 pixels apart):
 *
 *   0x100   a 4-pixel-high glyph before the number, lowered 3 pixels
 *   0x200   value 100 is drawn with three digits
 *   0x400   any other value is drawn with two digits, 6 pixels further right
 *   0x800   every digit drawn as glyph 0xBA instead
 *   0x1000  glyph 0xB4 before the number
 *   0x2000  glyph 0xC0 after the number
 *   0x8000  glyph 0xB4 before the number, 2 pixels to the left
 *
 * The 0x200 and 0x400 rules are skipped while numeric values are hidden.
 * High bytes 0xC00 and 0xE00 instead draw three placeholder glyphs (0xBA or
 * 0xD0), preceded by glyph 0xB4 when bit 0 is set. The cursor ends five
 * pixels past the last digit. `work` holds the high byte and later the final
 * cursor x; sharing one variable reproduces the target's register assignment,
 * as does mixing `src` and the global's own address in the glyph calls.
 * BATTLE links its own, longer body under this name.
 */
void text_draw_small_number_glyphs(s32 value, s32 format, u8* pixels, glyph_blit_t* pos) {
    glyph_blit_t* src = &g_text_small_number_glyph_rect;
    s32 work = format & 0xFF00;
    s32 i;

    if (work == 0xC00 || work == 0xE00) {
        g_text_small_number_glyph_rect.rect.y = 0x10;
        if (format & 1) {
            src->rect.x = 0xB4;
            text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, src, pos);
            pos->rect.x += 5;
        }
        src->rect.x = 0xBA;
        if (work == 0xE00) {
            src->rect.x = 0xD0;
        }
        for (i = 0; i < 3; i++) {
            text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_text_small_number_glyph_rect, pos);
            pos->rect.x += 5;
        }
        return;
    }
    if (g_menu_hide_numeric_values == 0) {
        if ((format & 0x200) && value == 100) {
            format = work + 3;
        }
        if ((format & 0x400) && value != 100) {
            format = (format & 0xFF00) + 2;
            pos->rect.x += 6;
        }
    }
    if (format & 0x1000) {
        src->rect.x = 0xB4;
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_text_small_number_glyph_rect, pos);
        pos->rect.x += 5;
    }
    if (format & 0x8000) {
        pos->rect.x -= 2;
        src->rect.x = 0xB4;
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_text_small_number_glyph_rect, pos);
        pos->rect.x += 7;
    }
    if (format & 0x100) {
        src->rect.x = 0x78;
        src->rect.y = 0x1A;
        src->rect.h = 4;
        pos->rect.y += 3;
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_text_small_number_glyph_rect, pos);
        pos->rect.x += 7;
        pos->rect.y -= 3;
        src->rect.y = 0x10;
        src->rect.h = 0xA;
    }
    pos->rect.x += ((format & 0xFF) - 1) * 5;
    work = pos->rect.x + 5;
    for (i = 0; i < (format & 0xFF); i++) {
        src->rect.x = value % 10 * 6 + 0x78;
        if (format & 0x800) {
            src->rect.x = 0xBA;
        }
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_text_small_number_glyph_rect, pos);
        value /= 10;
        pos->rect.x -= 5;
    }
    pos->rect.x = work;
    if (format & 0x2000) {
        src->rect.x = 0xC0;
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_text_small_number_glyph_rect, pos);
        pos->rect.x += 5;
    }
}
