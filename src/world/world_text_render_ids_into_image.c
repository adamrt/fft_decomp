#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Renders a -1 terminated list of text ids into a 4bpp image, one entry per
 * line of `rect` (x, y, width, line height). Bit 13 of an id draws eight
 * copies of glyph 0 instead of a text entry; bits 14-15 select the palette.
 * Control bytes: 0xF8 starts a new line, 0xFA adds a 4-pixel space, 0xE3 sets
 * a palette override, 0xE7 sets the x position, 0xFE/0xFF end the entry.
 * `i` is only assigned by the glyph-copy branch; the text branch compares
 * whatever it last held against `max_glyphs`, which reproduces the target.
 * `colors` and `colors2` are distinct copies of &ids[k]: one shared local
 * changes the register assignment. */
void world_text_render_ids_into_image(
    u8* image, RECT* rect, s32 unused, s32 max_glyphs, u8* font, s16* ids, s32 count, s16 glyph) {
    world_text_glyph_target_t target;
    s32 line;
    s32 i;
    s32 palette;
    s32 k;
    u8* text;
    s32 code;
    s16* colors;
    s16* colors2;

    line = 0;
    target.stride = rect->w;
    for (k = 0; ids[k] != -1; k++) {
        if (count == 0) {
            return;
        }
        target.x = rect->x;
        target.y = rect->y + line * rect->h;
        palette = 0;
        if (ids[k] & 0x2000) {
            i = 0;
            colors2 = &ids[k];
            for (; i < 8; i++) {
                target.x
                    += world_text_decode_font_glyph_to_4bpp(glyph, image, &target, ((s16)*colors2 >> 14) | palette);
            }
        } else {
            text = world_text_find_entry_by_index(font, ids[k] & 0xFFF, 2);
            code = *text++;
            if (code < 0xFE) {
                colors = &ids[k];
                do {
                    if (code == TEXT_NEWLINE || i >= max_glyphs) {
                        target.x = rect->x;
                        line++;
                        target.y = rect->y + line * rect->h;
                    }
                    if (code < TEXT_FORMAT_FIRST) {
                        if (code >= 0xD0) {
                            code &= 0xF;
                            code = code * 0xD0 + *text++;
                        }
                        target.x += world_text_decode_font_glyph_to_4bpp(
                            code, image, &target, ((s16)*colors >> 14) | palette);
                    } else if (code == TEXT_SPACE) {
                        target.x += 4;
                    } else if (code == TEXT_SET_X_POSITION) {
                        target.x = rect->x + *text++;
                    } else if (code == TEXT_SET_PALETTE) {
                        palette = *text++ >> 2;
                    }
                    code = *text++;
                } while (code < 0xFE);
            }
        }
        line++;
        count--;
    }
}
