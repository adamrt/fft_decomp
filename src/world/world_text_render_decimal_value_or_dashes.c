#include "fft/world.h"
#include "psx/types.h"

/*
 * Render a decimal value into a text image, right to left, as `flags & 0xff`
 * digit glyphs with optional sign/prefix glyphs selected by the high flag bits.
 *
 * Prefix modes 0xc00 and 0xe00 instead draw three placeholder glyphs (0xba or
 * 0xd0), optionally preceded by the 0xb4 glyph. The cursor ends five pixels
 * past the last digit. `work` holds the prefix mode and later the final cursor
 * x; sharing one variable reproduces the target's register assignment.
 */
void world_text_render_decimal_value_or_dashes(s32 value, s32 flags, void* pixels, world_glyph_blit_t* position) {
    world_glyph_blit_t* glyph;
    s32 work;
    s32 i;

    work = flags & 0xFF00;
    glyph = &g_world_text_large_number_glyph_rect;
    if (work == 0xC00 || work == 0xE00) {
        g_world_text_large_number_glyph_rect.rect.y = 0x10;
        if (flags & 1) {
            glyph->rect.x = 0xB4;
            world_text_blit_glyph(g_world_menu_glyph_sheet, pixels, glyph, position);
            position->rect.x += 5;
        }
        glyph->rect.x = 0xBA;
        if (work == 0xE00) {
            glyph->rect.x = 0xD0;
        }
        for (i = 0; i < 3; i++) {
            world_text_blit_glyph(g_world_menu_glyph_sheet, pixels, &g_world_text_large_number_glyph_rect, position);
            position->rect.x += 5;
        }
        return;
    }
    if (g_world_menu_hide_numeric_values == 0) {
        if ((flags & 0x200) && value == 100) {
            flags = work + 3;
        }
        if ((flags & 0x400) && value != 100) {
            flags = (flags & 0xFF00) + 2;
            position->rect.x += 6;
        }
    }
    if (flags & 0x1000) {
        glyph->rect.x = 0xB4;
        world_text_blit_glyph(g_world_menu_glyph_sheet, pixels, &g_world_text_large_number_glyph_rect, position);
        position->rect.x += 5;
    }
    if (flags & 0x8000) {
        position->rect.x -= 2;
        glyph->rect.x = 0xB4;
        world_text_blit_glyph(g_world_menu_glyph_sheet, pixels, &g_world_text_large_number_glyph_rect, position);
        position->rect.x += 7;
    }
    if (flags & 0x100) {
        glyph->rect.x = 0x78;
        glyph->rect.y = 0x1A;
        glyph->rect.h = 4;
        position->rect.y += 3;
        world_text_blit_glyph(g_world_menu_glyph_sheet, pixels, &g_world_text_large_number_glyph_rect, position);
        position->rect.x += 7;
        position->rect.y -= 3;
        glyph->rect.y = 0x10;
        glyph->rect.h = 0xA;
    }
    position->rect.x += ((flags & 0xFF) - 1) * 5;
    work = (s16)position->rect.x + 5;
    for (i = 0; i < (flags & 0xFF); i++) {
        glyph->rect.x = value % 10 * 6 + 0x78;
        if (flags & 0x800) {
            glyph->rect.x = 0xBA;
        }
        world_text_blit_glyph(g_world_menu_glyph_sheet, pixels, &g_world_text_large_number_glyph_rect, position);
        value /= 10;
        position->rect.x -= 5;
    }
    position->rect.x = work;
    if (flags & 0x2000) {
        glyph->rect.x = 0xC0;
        world_text_blit_glyph(g_world_menu_glyph_sheet, pixels, &g_world_text_large_number_glyph_rect, position);
        position->rect.x += 5;
    }
}
