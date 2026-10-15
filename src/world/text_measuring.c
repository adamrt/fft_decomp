/* Entry pixel width and the encoded character at a position. */

#include "fft/world.h"
#include "psx/types.h"

/* Sum the pixel widths of a 0xFE-terminated text entry. Bytes 0xD0..0xDF
 * introduce a two-byte glyph index; 0xFA advances a fixed four pixels and
 * 0xE7 carries an explicit one-byte advance. */
s32 world_text_measure_entry_pixel_width(const u8* text) {
    s32 width;
    s32 code;

    width = 0;
    while (*text != 0xFE) {
        code = *text;
        if (code < TEXT_FORMAT_FIRST) {
            if (code >= TEXT_EXTENDED_GLYPH_PREFIX_FIRST) {
                text++;
                code = ((code & 0xF) * TEXT_GLYPHS_PER_PREFIX) + *text;
            }
            width += g_text_glyph_widths[code];
        } else if (code == TEXT_SPACE) {
            width += 4;
        } else if (code == TEXT_SET_X_POSITION) {
            text++;
            width += *text;
        }
        text++;
    }
    return width;
}

/* Return the encoded character at a position inside a text-table entry.
 *
 * Lead bytes 0xd0..TEXT_EXTENDED_GLYPH_PREFIX_LAST introduce a two-byte code; everything else is a
 * single byte. */
s16 world_text_get_encoded_char_at_position(u8* table, s16 index, s32 position) {
    u8* cursor;
    s32 code;

    cursor = world_text_advance_entry_by_char_count(menu_text_find_entry_by_index(table, index, 2), position);
    code = *cursor++;
    if (code >= TEXT_EXTENDED_GLYPH_PREFIX_FIRST && code <= TEXT_EXTENDED_GLYPH_PREFIX_LAST) {
        code = *cursor | (code << 8);
    }
    return code;
}
