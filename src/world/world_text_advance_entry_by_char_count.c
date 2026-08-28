#include "psx/types.h"

/* Advance over count characters of a 0xFE-terminated text entry; bytes
 * 0xD0..0xDF carry a one-byte operand. */
u8* world_text_advance_entry_by_char_count(u8* text, s32 count) {
    s32 i;
    s32 offset;
    u8* cursor;

    offset = 0;
    for (i = 0; i < count; i++) {
        cursor = &text[offset];
        if (*cursor == 0xFE) {
            break;
        }
        if ((u8)(*cursor + 0x30) < 0x10) {
            offset += 2;
        } else {
            offset += 1;
        }
    }
    return text + offset;
}
