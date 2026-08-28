#include "fft/world.h"
#include "psx/types.h"

/* Return the encoded character at a position inside a text-table entry.
 *
 * Lead bytes 0xd0..0xdf introduce a two-byte code; everything else is a
 * single byte. */
s16 world_text_get_encoded_char_at_position(u8* table, s16 index, s32 position) {
    u8* cursor;
    s32 code;

    cursor = world_text_advance_entry_by_char_count(world_text_find_entry_by_index(table, index, 2), position);
    code = *cursor++;
    if ((u32)(code - 0xD0) < 0x10) {
        code = *cursor | (code << 8);
    }
    return code;
}
