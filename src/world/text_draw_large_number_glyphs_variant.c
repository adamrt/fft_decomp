#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "fft/battle_move.h"
#include "fft/battle_script.h"
#include "fft/battle_status.h"
#include "fft/battle_text.h"
#include "fft/battle_unit.h"
#include "fft/camera.h"
#include "fft/gfx.h"
#include "fft/main.h"
#include "fft/map.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/status.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/world.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/suzuki.h"
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
void text_draw_large_number_glyphs_variant(s32 value, s32 flags, u8* pixels, glyph_blit_t* position) {
    glyph_blit_t* glyph;
    s32 work;
    s32 i;

    work = flags & 0xFF00;
    glyph = &g_text_large_number_glyph_rect;
    if (work == 0xC00 || work == 0xE00) {
        g_text_large_number_glyph_rect.rect.y = 0x10;
        if (flags & 1) {
            glyph->rect.x = 0xB4;
            text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, glyph, position);
            position->rect.x += 5;
        }
        glyph->rect.x = 0xBA;
        if (work == 0xE00) {
            glyph->rect.x = 0xD0;
        }
        for (i = 0; i < 3; i++) {
            text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_text_large_number_glyph_rect, position);
            position->rect.x += 5;
        }
        return;
    }
    if (g_menu_hide_numeric_values == 0) {
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
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_text_large_number_glyph_rect, position);
        position->rect.x += 5;
    }
    if (flags & 0x8000) {
        position->rect.x -= 2;
        glyph->rect.x = 0xB4;
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_text_large_number_glyph_rect, position);
        position->rect.x += 7;
    }
    if (flags & 0x100) {
        glyph->rect.x = 0x78;
        glyph->rect.y = 0x1A;
        glyph->rect.h = 4;
        position->rect.y += 3;
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_text_large_number_glyph_rect, position);
        position->rect.x += 7;
        position->rect.y -= 3;
        glyph->rect.y = 0x10;
        glyph->rect.h = 0xA;
    }
    position->rect.x += ((flags & 0xFF) - 1) * 5;
    work = position->rect.x + 5;
    for (i = 0; i < (flags & 0xFF); i++) {
        glyph->rect.x = value % 10 * 6 + 0x78;
        if (flags & 0x800) {
            glyph->rect.x = 0xBA;
        }
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_text_large_number_glyph_rect, position);
        value /= 10;
        position->rect.x -= 5;
    }
    position->rect.x = work;
    if (flags & 0x2000) {
        glyph->rect.x = 0xC0;
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_text_large_number_glyph_rect, position);
        position->rect.x += 5;
    }
}
