#include "fft/battle_menu.h"
#include "fft/battle_text.h"
#include "fft/menu.h"

void menu_draw_number_glyphs(s32 value, s32 digits, u8* pixels, glyph_blit_t* pos) {
    s32 i;
    s32 cursor;
    glyph_blit_t* src;

    /* One variable holds the sign-glyph selector first and the x to leave the
     * cursor at afterwards; the target keeps both roles in $s6. */
    src = &g_menu_number_glyph;
    g_menu_number_glyph.rect.h = 0xF;
    cursor = digits & 0xFF00;
    if (cursor == 0xC00) {
        pos->rect.x += 2;
        src->rect.x = 0xBA;
        g_menu_number_glyph.rect.y = 0xD;
        g_menu_number_glyph.rect.w = 6;
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, src, pos);
        pos->rect.x += 7;
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, src, pos);
        pos->rect.x += 7;
        g_menu_number_glyph.rect.w = 8;
        return;
    }
    if (digits & 0x1000) {
        src->rect.x = 0xC8;
        g_menu_number_glyph.rect.y = 0;
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, src, pos);
        pos->rect.x += 7;
    }
    if ((digits & 0x800) || (digits & 0x400)) {
        if (digits & 0x400) {
            src->rect.x = 0xC8;
        } else {
            src->rect.x = 0xBA;
        }
        src->rect.y = 0xD;
        src->rect.w = 6;
        pos->rect.x -= 6;
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_menu_number_glyph, pos);
        pos->rect.x += 6;
        src->rect.w = 8;
    }
    /* Not a variable: the target recomputes the digit count at the loop test
     * rather than keeping it live, so each use spells the mask out. */
    pos->rect.x += ((digits & 0xFF) - 1) * 7;
    cursor = pos->rect.x + 7;
    for (i = 0; i < (digits & 0xFF); i++) {
        src->rect.y = 0;
        src->rect.x = (value % 10) * 8 + 0x78;
        if (g_menu_hide_numeric_values != 0) {
            src->rect.x = 0x48;
            src->rect.y = 0x10;
        }
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_menu_number_glyph, pos);
        /* After the call: the quotient stays live past the argument setup, so
         * it takes a callee-saved register and the update lands in the delay
         * slot, as the target does at 0x238. */
        value /= 10;
        pos->rect.x -= 7;
    }
    pos->rect.x = cursor;
}
