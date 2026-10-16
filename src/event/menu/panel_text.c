/* Status panel primitive colors and signed decimal value rendering. */

#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/menu_gfx.h"
#include "fft/menu_input.h"
#include "fft/menu_panel.h"
#include "fft/menu_text.h"
#include "psx/libgpu.h"
#include "psx/types.h"

static void menu_text_render_decimal_value(s32 value, s32 flags, u8* pixels, glyph_blit_t* position);

/* Set the status panel's sprite CLUTs and the tint of its last five sprites: the dimmed
 * set with a blue tint for style 1 or while menu input lock mode 1 is on, otherwise the
 * normal set with a neutral 0x80 tint. */
void menu_set_status_panel_primitive_colors(menu_status_panel_buffer_t* primitives, const menu_status_thread_t* state) {
    s32 i;
    s32 clut;

    if (state->style == 1 || g_menu_input_lock_mode == 1) {
        clut = 0x7dfc;
        for (i = 11; i >= 0; i--) {
            (primitives->sprites + i)->clut = clut;
        }
        for (i = 12; i < 19; i++) {
            (primitives->sprites + i)->clut = MENU_CLUT_DIMMED;
        }
        primitives->sprites[0].clut = 0x7c7c;
        primitives->sprites[1].clut = 0x7c7c;
        for (i = 0; i < 5; i++) {
            SetShadeTex(&primitives->sprites[i + 19], 0);
            (primitives->sprites + i + 19)->r0 = 0x40;
            (primitives->sprites + i + 19)->g0 = 0x40;
            (primitives->sprites + i + 19)->b0 = 0x80;
        }
    } else {
        clut = 0x7d7c;
        for (i = 11; i >= 0; i--) {
            (primitives->sprites + i)->clut = clut;
        }
        for (i = 12; i < 19; i++) {
            (primitives->sprites + i)->clut = MENU_CLUT_NORMAL;
        }
        primitives->sprites[0].clut = 0x7cbc;
        primitives->sprites[1].clut = 0x7cbc;
        for (i = 0; i < 5; i++) {
            SetShadeTex(&primitives->sprites[i + 19], 0);
            (primitives->sprites + i + 19)->r0 = 0x80;
            (primitives->sprites + i + 19)->g0 = 0x80;
            (primitives->sprites + i + 19)->b0 = 0x80;
        }
    }
}

/* Select the menu primitives' normal or dimmed colors: dimmed for panel style 1 or while menu
 * input lock mode 1 is on. The palette initializer receives `primitives` unchanged in $a0. */
void menu_select_status_primitive_color_palette(
    menu_palette_primitives_t* primitives, const menu_status_thread_t* thread) {
    if (thread->style == 1 || g_menu_input_lock_mode == 1) {
        menu_init_primitive_colors_dimmed(primitives);
    } else {
        menu_init_primitive_colors_normal(primitives);
    }
}

/* Draw each entry's value as a signed change: a negative value with a minus sign over background
 * color 8, a positive one with a plus sign over color 0xC, and zero as a lone minus sign. The
 * entries' format sign flags are rewritten to match. */
void menu_text_render_signed_decimal_entries(u8* pixels, menu_number_entry_t* entries, glyph_blit_t* out, s32 count) {
    s32 i;
    s32 amount;

    for (i = 0; i < count; entries++, i++) {
        amount = *entries->value;
        out->rect.x = entries->x;
        out->rect.y = entries->y;
        entries->format &= MENU_DECIMAL_SIGN_CLEAR_MASK;
        if (amount < 0) {
            out->color = 0x88888888;
            amount = -amount;
            entries->format |= MENU_DECIMAL_SIGN_MINUS;
        } else if (amount > 0) {
            out->color = 0xcccccccc;
            entries->format |= MENU_DECIMAL_SIGN_PLUS;
        } else {
            out->color = 0;
            entries->format |= MENU_DECIMAL_SIGN_MINUS;
        }
        menu_text_render_decimal_value(amount, entries->format, pixels, out);
        out->color = 0;
    }
}

/* Draw each entry's value as menu_text_render_signed_decimal_entries does, with background colors
 * 7 and 0xB, leaving the entries unchanged and drawing a zero as the placeholder format. */
void menu_text_render_decimal_entry_list(u8* pixels, const menu_number_entry_t* entries, glyph_blit_t* out, s32 count) {
    s32 i;

    for (i = 0; i < count; entries++, i++, out->color = 0) {
        s32 amount = *entries->value;
        s32 flags;
        out->rect.x = entries->x;
        out->rect.y = entries->y;
        flags = entries->format;
        flags &= MENU_DECIMAL_SIGN_CLEAR_MASK;
        if (amount < 0) {
            amount = -amount;
            flags |= MENU_DECIMAL_SIGN_MINUS;
            out->color = 0x77777777;
        } else if (amount > 0) {
            flags |= MENU_DECIMAL_SIGN_PLUS;
            out->color = 0xbbbbbbbb;
        } else {
            out->color = 0;
            flags &= MENU_DECIMAL_FIELD_WIDTH_CLEAR_MASK;
            flags |= MENU_DECIMAL_ZERO_PLACEHOLDER_FLAGS;
        }
        menu_text_render_decimal_value(amount, flags, pixels, out);
    }
}

/* Render a decimal value into a text image at `position`.
 *
 * The low nibble of `flags` is the field width in digit columns of 5 pixels;
 * the MENU_DECIMAL_* bits add a slash or dotted prefix glyph, a minus or plus
 * sign and leading zeros, and widen the field for the value 100. A zero with
 * the minus flag draws only the sign glyph. Digits are drawn right to left,
 * at most the low byte of `flags` of them. */
static void menu_text_render_decimal_value(s32 value, s32 flags, u8* pixels, glyph_blit_t* position) {
    s32 saved_color = position->color;
    s32 plus;
    s32 digits;
    s32 min_digits;
    s32 i;

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
