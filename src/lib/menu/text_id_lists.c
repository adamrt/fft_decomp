/* Text entry seek and -1 terminated text-id list image rendering. */

#include "fft/text.h"
#include "psx/types.h"

/* Skip count text entries and return the start of the next one: each entry ends with a 0xFE or
 * 0xFF byte, and the byte after an extended-glyph prefix is skipped so it is never taken for a
 * terminator. */
const u8* menu_text_find_entry_by_index(const u8* data, s16 count, s32 unused_2) {
    while (count != 0) {
        u8 value = *data++;

        if (value >= TEXT_EXTENDED_GLYPH_PREFIX_FIRST) {
            if (value <= TEXT_EXTENDED_GLYPH_PREFIX_LAST) {
                data++;
            } else if (value >= TEXT_END_WAIT_FOR_CONFIRM) {
                count--;
            }
        }
    }
    return data;
}

/* Renders a -1 terminated list of text ids into a 4bpp image, one entry per
 * line of `bounds` (x, y, width, line height). Bit 13 of an id draws eight
 * copies of `glyph` instead of a text entry; bits 14-15 select the style.
 * Control bytes: 0xF8 starts a new line, 0xFA adds a 4-pixel space, 0xE3 sets
 * a style override, 0xE7 sets the x position, 0xFE/0xFF end the entry.
 * `col` is only assigned by the glyph-copy branch; the text branch compares
 * whatever it last held against `max_chars`, which reproduces the target.
 * `colors` and `colors2` are distinct copies of &text_ids[k]: one shared local
 * changes the register assignment. The caller also passes its row flags as a
 * ninth argument, which is never read. */
void menu_text_render_encoded_ids_to_image(u8* image, const text_image_bounds_t* bounds, s32 unused, s32 max_chars,
    const u8* text_table, const s16* text_ids, s32 count, s16 glyph, s32 unused_flags) {
    text_image_position_t position;
    s32 line;
    s32 col;
    s32 style;
    s32 k;
    const u8* text;
    s32 code;
    const s16* colors;
    const s16* colors2;

    line = 0;
    position.row_stride = bounds->row_stride;
    for (k = 0; text_ids[k] != -1; k++) {
        if (count == 0) {
            return;
        }
        position.x = bounds->x;
        position.y = bounds->y + line * bounds->line_height;
        style = 0;
        if (text_ids[k] & 0x2000) {
            col = 0;
            colors2 = &text_ids[k];
            for (; col < 8; col++) {
                position.x += menu_text_render_glyph_to_4bpp_image(glyph, image, &position, (*colors2 >> 14) | style);
            }
        } else {
            text = menu_text_find_entry_by_index(text_table, text_ids[k] & 0xFFF, 2);
            code = *text++;
            if (code < 0xFE) {
                colors = &text_ids[k];
                do {
                    if (code == TEXT_NEWLINE || col >= max_chars) {
                        position.x = bounds->x;
                        line++;
                        position.y = bounds->y + line * bounds->line_height;
                    }
                    if (code < TEXT_FORMAT_FIRST) {
                        if (code >= TEXT_EXTENDED_GLYPH_PREFIX_FIRST) {
                            code &= 0xF;
                            code = code * TEXT_GLYPHS_PER_PREFIX + *text++;
                        }
                        position.x
                            += menu_text_render_glyph_to_4bpp_image(code, image, &position, (*colors >> 14) | style);
                    } else if (code == TEXT_SPACE) {
                        position.x += 4;
                    } else if (code == TEXT_SET_X_POSITION) {
                        position.x = bounds->x + *text++;
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
