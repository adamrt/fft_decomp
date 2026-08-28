#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* This site passes world_text_blit_glyph's second argument as an integer and
 * the source as a RECT, so it calls through this type. */
typedef void (*world_glyph_blit_fn)(void*, s32, const void*, world_glyph_blit_t*);

/* Draws `value` right to left as `digits & 0xff` 8-pixel glyphs at `pos`,
 * preceded by sign glyphs selected by the flag bits of `digits`: 0xC00
 * draws two glyphs, 0x1000 one at 0xC8, 0x800/0x400 a 0xC8 or 0xBA glyph
 * six pixels to the left. The cursor is left after the number. */
void world_draw_menu_number_glyphs(s32 value, s32 digits, void* resource, world_glyph_blit_t* pos) {
    s32 i;
    s32 cursor;
    RECT* src;

    /* One variable holds the sign-glyph selector first and the x to leave the
     * cursor at afterwards; the target keeps both roles in $s6. */
    src = &g_world_menu_number_glyph_rect;
    g_world_menu_number_glyph_rect.h = 0xF;
    cursor = digits & 0xFF00;
    if (cursor == 0xC00) {
        pos->rect.x += 2;
        src->x = 0xBA;
        g_world_menu_number_glyph_rect.y = 0xD;
        g_world_menu_number_glyph_rect.w = 6;
        ((world_glyph_blit_fn)world_text_blit_glyph)((void*)g_world_menu_glyph_sheet, (s32)resource, src, pos);
        pos->rect.x += 7;
        ((world_glyph_blit_fn)world_text_blit_glyph)((void*)g_world_menu_glyph_sheet, (s32)resource, src, pos);
        pos->rect.x += 7;
        g_world_menu_number_glyph_rect.w = 8;
        return;
    }
    if (digits & 0x1000) {
        src->x = 0xC8;
        g_world_menu_number_glyph_rect.y = 0;
        ((world_glyph_blit_fn)world_text_blit_glyph)((void*)g_world_menu_glyph_sheet, (s32)resource, src, pos);
        pos->rect.x += 7;
    }
    if ((digits & 0x800) || (digits & 0x400)) {
        if (digits & 0x400) {
            src->x = 0xC8;
        } else {
            src->x = 0xBA;
        }
        src->y = 0xD;
        src->w = 6;
        pos->rect.x -= 6;
        ((world_glyph_blit_fn)world_text_blit_glyph)(
            (void*)g_world_menu_glyph_sheet, (s32)resource, &g_world_menu_number_glyph_rect, pos);
        pos->rect.x += 6;
        src->w = 8;
    }
    /* Not a variable: the target recomputes the digit count at the loop test
     * rather than keeping it live, so each use spells the mask out. */
    pos->rect.x += ((digits & 0xFF) - 1) * 7;
    cursor = pos->rect.x + 7;
    for (i = 0; i < (digits & 0xFF); i++) {
        src->y = 0;
        src->x = (value % 10) * 8 + 0x78;
        if (g_world_menu_hide_numeric_values != 0) {
            src->x = 0x48;
            src->y = 0x10;
        }
        ((world_glyph_blit_fn)world_text_blit_glyph)(
            (void*)g_world_menu_glyph_sheet, (s32)resource, &g_world_menu_number_glyph_rect, pos);
        /* After the call: the quotient stays live past the argument setup, so
         * it takes a callee-saved register and the update lands in the delay
         * slot, as the target does at 0x238. */
        value /= 10;
        pos->rect.x -= 7;
    }
    pos->rect.x = cursor;
}
