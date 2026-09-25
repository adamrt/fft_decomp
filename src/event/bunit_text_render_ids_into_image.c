#include "fft/battle_text.h"
#include "fft/bunit.h"
#include "fft/text.h"
#include "psx/types.h"

/* Renders a -1 terminated list of text ids into a 4bpp image, one entry per
 * line of `rect` (x, y, width, line height). Bit 13 of an id draws eight
 * copies of `glyph` instead of a text entry; bits 14-15 select the style.
 * Control bytes: 0xF8 starts a new line, 0xFA adds a 4-pixel space, 0xE3 sets
 * a style override, 0xE7 sets the x position, 0xFE/0xFF end the entry.
 * `col` is only assigned by the glyph-copy branch; the text branch compares
 * whatever it last held against `max_chars`, which reproduces the target.
 * `colors` and `colors2` are distinct copies of &ids[k]: one shared local
 * changes the register assignment. The caller also passes its row flags as a
 * ninth argument, which is never read. */
void bunit_text_render_ids_into_image(u8* image, battle_menu_text_image_bounds_t* rect, s32 unused, s32 max_chars,
    u8* font, s16* ids, s32 count, s16 glyph, s32 unused_flags) {
    bunit_text_image_position_t position;
    s32 line;
    s32 col;
    s32 style;
    s32 k;
    const u8* text;
    s32 code;
    s16* colors;
    s16* colors2;

    line = 0;
    position.row_stride = rect->row_stride;
    for (k = 0; ids[k] != -1; k++) {
        if (count == 0) {
            return;
        }
        position.x = rect->x;
        position.y = rect->y + line * rect->line_height;
        style = 0;
        if (ids[k] & 0x2000) {
            col = 0;
            colors2 = &ids[k];
            for (; col < 8; col++) {
                position.x
                    += bunit_text_render_glyph_to_4bpp_image(glyph, image, &position, ((s16)*colors2 >> 14) | style);
            }
        } else {
            text = ((const u8* (*)(u8*, s32, s32))bunit_text_skip_encoded_segments)(font, ids[k] & 0xFFF, 2);
            code = *text++;
            if (code < 0xFE) {
                colors = &ids[k];
                do {
                    if (code == TEXT_NEWLINE || col >= max_chars) {
                        position.x = rect->x;
                        line++;
                        position.y = rect->y + line * rect->line_height;
                    }
                    if (code < TEXT_FORMAT_FIRST) {
                        if (code >= 0xD0) {
                            code &= 0xF;
                            code = code * 0xD0 + *text++;
                        }
                        position.x += bunit_text_render_glyph_to_4bpp_image(
                            code, image, &position, ((s16)*colors >> 14) | style);
                    } else if (code == TEXT_SPACE) {
                        position.x += 4;
                    } else if (code == TEXT_SET_X_POSITION) {
                        position.x = rect->x + *text++;
                    } else if (code == TEXT_SET_PALETTE) {
                        style = *text++ >> 2;
                    }
                    code = *text++;
                } while (code < 0xFE);
            }
        }
        line++;
        count--;
    }
}
