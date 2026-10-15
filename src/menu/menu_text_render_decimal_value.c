#include "fft/battle_text.h"
#include "fft/menu.h"
#include "psx/types.h"

/* Render a decimal value into a text image at `position`.
 *
 * The low nibble of `flags` is the field width in digit columns of 5 pixels;
 * the MENU_DECIMAL_* bits add a slash or dotted prefix glyph, a minus or plus
 * sign and leading zeros, and widen the field for the value 100. A zero with
 * the minus flag draws only the sign glyph. Digits are drawn right to left,
 * at most the low byte of `flags` of them. */
void menu_text_render_decimal_value(s32 value, s32 flags, u8* pixels, glyph_blit_t* position) {
    s32 saved_color;
    s32 plus;
    s32 digits;
    s32 min_digits;
    s32 i;

    saved_color = position->color;
    position->color = 0;
    if ((flags & MENU_DECIMAL_WIDEN_FOR_100) && value == 100) {
        flags = (flags & 0xff00) + 3;
    }
    if (flags & MENU_DECIMAL_SLASH_PREFIX) {
        position->rect.x -= 2;
        g_menu_decimal_glyph.rect.x = 0xb4;
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_menu_decimal_glyph, position);
        position->rect.x += 7;
    }
    if (flags & MENU_DECIMAL_DOTS_PREFIX) {
        g_menu_decimal_glyph.rect.x = 0x78;
        g_menu_decimal_glyph.rect.y = 0x1a;
        g_menu_decimal_glyph.rect.h = 4;
        position->rect.y += 3;
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_menu_decimal_glyph, position);
        position->rect.x += 7;
        position->rect.y -= 3;
        g_menu_decimal_glyph.rect.y = 0x10;
        g_menu_decimal_glyph.rect.h = 0x0a;
    }
    position->color = saved_color;
    if (flags & MENU_DECIMAL_SIGN_MINUS) {
        g_menu_decimal_glyph.rect.x = 0xba;
        if (value == 0) {
            if ((flags & MENU_DECIMAL_FIELD_WIDTH_MASK) == 4) {
                position->rect.x += 10;
            } else {
                position->rect.x += 5;
            }
        }
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_menu_decimal_glyph, position);
        position->rect.x += 5;
        if (value == 0) {
            return;
        }
    }
    plus = flags & MENU_DECIMAL_SIGN_PLUS;
    if (plus) {
        g_menu_decimal_glyph.rect.x = 0xc8;
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_menu_decimal_glyph, position);
        position->rect.x += 5;
    }
    digits = flags & MENU_DECIMAL_FIELD_WIDTH_MASK;
    if (flags & MENU_DECIMAL_THREE_COLUMN_FIELD) {
        digits = 3;
    }
    if (plus) {
        digits--;
    }
    if (flags & MENU_DECIMAL_SIGN_MINUS) {
        digits--;
    }
    /* Retail gives every rung the same width, so the value is always placed as one digit. */
    if (value < 10) {
        min_digits = 1;
    } else if (value < 100) {
        min_digits = 1;
    } else {
        min_digits = 1;
    }
    if ((flags & MENU_DECIMAL_FIELD_WIDTH_MASK) != 1) {
        position->rect.x += (digits - min_digits) * 5;
    }
    for (i = 0; i < (flags & 0xff); i++) {
        g_menu_decimal_glyph.rect.x = value % 10 * 6 + 0x78;
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_menu_decimal_glyph, position);
        position->rect.x -= 5;
        value /= 10;
        if (!(flags & MENU_DECIMAL_LEADING_ZEROS) && value == 0) {
            break;
        }
    }
    position->color = 0;
}
