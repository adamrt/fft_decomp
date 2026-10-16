#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_text.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/*
 * Render a decimal value into a text image, right to left, as `flags & 0xff`
 * digit glyphs with optional sign/prefix glyphs selected by the high flag bits.
 *
 * Prefix modes 0xc00 and 0xe00 instead draw three placeholder glyphs (0xba or
 * 0xd0), optionally preceded by the 0xb4 glyph. The cursor ends five pixels
 * past the last digit. `work` holds the prefix mode and later the final cursor
 * x; sharing one variable reproduces the target's register assignment.
 * BATTLE links its own, longer body under this name.
 */
void text_draw_large_number_glyphs_variant(s32 value, s32 digits, u8* pixels, glyph_blit_t* pos) {
    glyph_blit_t* src;
    s32 work = digits & 0xFF00;
    s32 i;

    src = &g_text_large_number_glyph_rect;
    if (work == 0xC00 || work == 0xE00) {
        g_text_large_number_glyph_rect.rect.y = 0x10;
        if (digits & 1) {
            src->rect.x = 0xB4;
            text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, src, pos);
            pos->rect.x += 5;
        }
        src->rect.x = 0xBA;
        if (work == 0xE00) {
            src->rect.x = 0xD0;
        }
        for (i = 0; i < 3; i++) {
            text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_text_large_number_glyph_rect, pos);
            pos->rect.x += 5;
        }
        return;
    }
    if (g_menu_hide_numeric_values == 0) {
        if ((digits & 0x200) && value == 100) {
            digits = work + 3;
        }
        if ((digits & 0x400) && value != 100) {
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
    work = pos->rect.x + 5;
    for (i = 0; i < (digits & 0xFF); i++) {
        src->rect.x = value % 10 * 6 + 0x78;
        if (digits & 0x800) {
            src->rect.x = 0xBA;
        }
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_text_large_number_glyph_rect, pos);
        value /= 10;
        pos->rect.x -= 5;
    }
    pos->rect.x = work;
    if (digits & 0x2000) {
        src->rect.x = 0xC0;
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_text_large_number_glyph_rect, pos);
        pos->rect.x += 5;
    }
}
