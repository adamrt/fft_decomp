#include "fft/battle.h"
#include "psx/gpu.h"
#include "psx/types.h"

typedef void (*battle_glyph_blit_fn)(void*, s32, const void*, battle_rect_t*);

/* Draws `value` right to left as `digits & 0xff` large glyphs at `pos`, with
 * the sign and marker glyphs selected by the flag bits of `digits`. Sibling of
 * battle_draw_menu_number_glyphs (0x8014ac30) on the 5-pixel font. */
void battle_text_draw_large_number_glyphs(s32 value, s32 digits, void* resource, battle_rect_t* pos) {
    s32 i;
    s32 cursor;
    RECT* src;

    src = &g_battle_text_large_number_glyph_rect;
    cursor = digits & 0xFF00;
    if (cursor == 0xC00 || cursor == 0xE00) {
        g_battle_text_large_number_glyph_rect.y = 0x10;
        if (digits & 1) {
            src->x = 0xB4;
            ((battle_glyph_blit_fn)blit_text_glyph)((void*)g_battle_menu_glyph_image, (s32)resource, src, pos);
            pos->x += 5;
        }
        src->x = 0xBA;
        if (cursor == 0xE00) {
            src->x = 0xD0;
        }
        for (i = 0; i < 3; i++) {
            ((battle_glyph_blit_fn)blit_text_glyph)(
                (void*)g_battle_menu_glyph_image, (s32)resource, &g_battle_text_large_number_glyph_rect, pos);
            pos->x += 5;
        }
        return;
    }
    if (digits & 0x800) {
        src->x = 0xBA;
        ((battle_glyph_blit_fn)blit_text_glyph)((void*)g_battle_menu_glyph_image, (s32)resource, src, pos);
        pos->x += 7;
    }
    if (g_battle_menu_hide_numeric_values == 0) {
        if ((digits & 0x200) && value == 0x64) {
            digits = cursor + 3;
        }
        if ((digits & 0x400) && value != 0x64) {
            digits = (digits & 0xFF00) + 2;
            pos->x += 6;
        }
    }
    if (digits & 0x1000) {
        src->x = 0xB4;
        ((battle_glyph_blit_fn)blit_text_glyph)(
            (void*)g_battle_menu_glyph_image, (s32)resource, &g_battle_text_large_number_glyph_rect, pos);
        pos->x += 5;
    }
    if (digits & 0x8000) {
        pos->x -= 2;
        src->x = 0xB4;
        ((battle_glyph_blit_fn)blit_text_glyph)(
            (void*)g_battle_menu_glyph_image, (s32)resource, &g_battle_text_large_number_glyph_rect, pos);
        pos->x += 7;
    }
    if (digits & 0x100) {
        src->x = 0x78;
        src->y = 0x1A;
        src->h = 4;
        pos->y += 3;
        ((battle_glyph_blit_fn)blit_text_glyph)(
            (void*)g_battle_menu_glyph_image, (s32)resource, &g_battle_text_large_number_glyph_rect, pos);
        pos->x += 7;
        pos->y -= 3;
        src->y = 0x10;
        src->h = 0xA;
    }
    pos->x += ((digits & 0xFF) - 1) * 5;
    cursor = pos->x + 5;
    for (i = 0; i < (digits & 0xFF); i++) {
        src->x = (value % 10) * 6 + 0x78;
        if (g_battle_menu_hide_numeric_values != 0) {
            src->x = 0xD0;
        }
        ((battle_glyph_blit_fn)blit_text_glyph)(
            (void*)g_battle_menu_glyph_image, (s32)resource, &g_battle_text_large_number_glyph_rect, pos);
        value /= 10;
        pos->x -= 5;
    }
    pos->x = cursor;
    if (digits & 0x2000) {
        src->x = 0xC0;
        ((battle_glyph_blit_fn)blit_text_glyph)(
            (void*)g_battle_menu_glyph_image, (s32)resource, &g_battle_text_large_number_glyph_rect, pos);
        pos->x += 5;
    }
}
