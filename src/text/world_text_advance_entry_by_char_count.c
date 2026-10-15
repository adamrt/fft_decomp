#include "fft/world.h"
#include "psx/types.h"

/* Advance over count characters of a 0xFE-terminated text entry; bytes
 * TEXT_EXTENDED_GLYPH_PREFIX_FIRST..TEXT_EXTENDED_GLYPH_PREFIX_LAST carry a one-byte operand. */
u8* world_text_advance_entry_by_char_count(u8* text, s32 count) {
    s32 i;
    s32 offset;
    u8* cursor;

    offset = 0;
    for (i = 0; i < count; i++) {
        cursor = &text[offset];
        if (*cursor == TEXT_END_WAIT_FOR_CONFIRM) {
            break;
        }
        if (*cursor >= TEXT_EXTENDED_GLYPH_PREFIX_FIRST && *cursor <= TEXT_EXTENDED_GLYPH_PREFIX_LAST) {
            offset += 2;
        } else {
            offset += 1;
        }
    }
    return text + offset;
}
