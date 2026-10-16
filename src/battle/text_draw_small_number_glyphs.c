/* Drawing a number right to left in the small 6x10 number glyphs, with sign and marker glyphs. */

#include "fft/gfx.h"
#include "fft/menu_gfx.h"
#include "fft/text.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Draws `value` right to left as `format & 0xff` small 6x10 glyphs at `pos`, with
 * the sign and marker glyphs selected by the flag bits of `format`. Sibling of
 * menu_draw_large_number_glyphs (0x8014ac30), which draws 8x15 glyphs. WORLD links its
 * own body under this name: it lacks the leading 0x800 glyph and the hidden
 * value check, and draws every digit as 0xba under flag 0x800 instead.
 *
 * The high byte of `format` selects the extras (glyphs named by their x in the
 * menu glyph sheet, where the digits sit 6 pixels apart from x 0x78; they
 * draw 5 pixels apart):
 *   0xc00, 0xe00  the whole number becomes three 0xba or 0xd0 placeholders,
 *                 after an 0xb4 glyph when bit 0 is set
 *   0x800         a leading 0xba glyph
 *   0x200         the value 100 widens to three digits (brave and faith use 0x202)
 *   0x400         values other than 100 draw two digits, 6 pixels further
 *                 right so they line up with a three-digit 100
 *   0x1000        a leading 0xb4 glyph; 0x8000 draws it 2 pixels further left
 *   0x100         a short glyph (4 rows from y 0x1a) lowered by 3 pixels
 *   0x2000        a trailing 0xc0 glyph
 * While numeric values are hidden every digit draws as 0xd0 and the 0x200 and
 * 0x400 width rules are skipped. The pen ends 5 pixels past the last digit. */
void text_draw_small_number_glyphs(s32 value, s32 format, u8* pixels, glyph_blit_t* pos) {
    s32 i;
    s32 cursor;
    glyph_blit_t* src = &g_text_small_number_glyph_rect;

    /* `cursor` holds the format's high byte first and the pen's final x
     * later; giving the two roles separate locals changes register use. */
    cursor = format & 0xFF00;
    if (cursor == 0xC00 || cursor == 0xE00) {
        g_text_small_number_glyph_rect.rect.y = 0x10;
        if (format & 1) {
            src->rect.x = 0xB4;
            text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, src, pos);
            pos->rect.x += 5;
        }
        src->rect.x = 0xBA;
        if (cursor == 0xE00) {
            src->rect.x = 0xD0;
        }
        for (i = 0; i < 3; i++) {
            text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_text_small_number_glyph_rect, pos);
            pos->rect.x += 5;
        }
        return;
    }
    if (format & 0x800) {
        src->rect.x = 0xBA;
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, src, pos);
        pos->rect.x += 7;
    }
    if (g_unit_panel_hide_numeric_values == 0) {
        if ((format & 0x200) && value == 100) {
            format = cursor + 3;
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
    cursor = pos->rect.x + 5;
    for (i = 0; i < (format & 0xFF); i++) {
        src->rect.x = (value % 10) * 6 + 0x78;
        if (g_unit_panel_hide_numeric_values != 0) {
            src->rect.x = 0xD0;
        }
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_text_small_number_glyph_rect, pos);
        value /= 10;
        pos->rect.x -= 5;
    }
    pos->rect.x = cursor;
    if (format & 0x2000) {
        src->rect.x = 0xC0;
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_text_small_number_glyph_rect, pos);
        pos->rect.x += 5;
    }
}
