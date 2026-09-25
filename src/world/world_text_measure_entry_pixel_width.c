#include "fft/world.h"
#include "psx/types.h"

/* Sum the pixel widths of a 0xFE-terminated text entry. Bytes 0xD0..0xDF
 * introduce a two-byte glyph index; 0xFA advances a fixed four pixels and
 * 0xE7 carries an explicit one-byte advance. */
s32 world_text_measure_entry_pixel_width(u8* text) {
    s32 width;
    s32 code;

    width = 0;
    while (*text != 0xFE) {
        code = *text;
        if (code < TEXT_FORMAT_FIRST) {
            if (code >= 0xD0) {
                text++;
                code = ((code & 0xF) * 0xD0) + *text;
            }
            width += g_world_text_glyph_widths[code];
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
