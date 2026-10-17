/* Drawing a number right to left in the small 6x10 number glyphs, with sign and marker glyphs. */

#include "fft/gfx.h"
#include "fft/menu_gfx.h"
#include "fft/text.h"
#include "fft/unit_panel.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Draw the low-byte digit count right to left, with the prefixes, suffix and
 * width rules of small_number_format_flag_e. The source digits are six pixels
 * apart, but the output advances five pixels per digit. Placeholder modes
 * draw three dashes or question marks, optionally preceded by a slash.
 *
 * BATTLE draws a dash prefix under SMALL_NUMBER_DASH. Hidden numeric values
 * replace every digit with a question mark and suppress width adjustments.
 * WORLD has its own body with different dash and hidden-value behavior. */
void text_draw_small_number_glyphs(s32 value, s32 format, u8* pixels, glyph_blit_t* pos) {
    s32 i;
    s32 cursor;
    glyph_blit_t* src = &g_text_small_number_glyph_rect;

    /* `cursor` holds the format's high byte first and the pen's final x
     * later; giving the two roles separate locals changes register use. */
    cursor = format & SMALL_NUMBER_FORMAT_MASK;
    if (cursor == SMALL_NUMBER_PLACEHOLDER_DASHES || cursor == SMALL_NUMBER_PLACEHOLDER_QUESTIONS) {
        g_text_small_number_glyph_rect.rect.y = 0x10;
        if (format & SMALL_NUMBER_PLACEHOLDER_SLASH) {
            src->rect.x = 0xB4;
            text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, src, pos);
            pos->rect.x += 5;
        }
        src->rect.x = 0xBA;
        if (cursor == SMALL_NUMBER_PLACEHOLDER_QUESTIONS) {
            src->rect.x = 0xD0;
        }
        for (i = 0; i < 3; i++) {
            text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_text_small_number_glyph_rect, pos);
            pos->rect.x += 5;
        }
        return;
    }
    if (format & SMALL_NUMBER_DASH) {
        src->rect.x = 0xBA;
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, src, pos);
        pos->rect.x += 7;
    }
    if (g_unit_panel_hide_numeric_values == 0) {
        if ((format & SMALL_NUMBER_WIDEN_FOR_100) && value == 100) {
            format = cursor + 3;
        }
        if ((format & SMALL_NUMBER_ALIGN_TWO_DIGITS) && value != 100) {
            format = (format & SMALL_NUMBER_FORMAT_MASK) + 2;
            pos->rect.x += 6;
        }
    }
    if (format & SMALL_NUMBER_SLASH_PREFIX) {
        src->rect.x = 0xB4;
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_text_small_number_glyph_rect, pos);
        pos->rect.x += 5;
    }
    if (format & SMALL_NUMBER_SLASH_PREFIX_LEFT) {
        pos->rect.x -= 2;
        src->rect.x = 0xB4;
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_text_small_number_glyph_rect, pos);
        pos->rect.x += 7;
    }
    if (format & SMALL_NUMBER_DOTS_PREFIX) {
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
    pos->rect.x += ((format & SMALL_NUMBER_DIGIT_COUNT_MASK) - 1) * 5;
    cursor = pos->rect.x + 5;
    for (i = 0; i < (format & SMALL_NUMBER_DIGIT_COUNT_MASK); i++) {
        src->rect.x = (value % 10) * 6 + 0x78;
        if (g_unit_panel_hide_numeric_values != 0) {
            src->rect.x = 0xD0;
        }
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_text_small_number_glyph_rect, pos);
        value /= 10;
        pos->rect.x -= 5;
    }
    pos->rect.x = cursor;
    if (format & SMALL_NUMBER_PERCENT_SUFFIX) {
        src->rect.x = 0xC0;
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_text_small_number_glyph_rect, pos);
        pos->rect.x += 5;
    }
}
