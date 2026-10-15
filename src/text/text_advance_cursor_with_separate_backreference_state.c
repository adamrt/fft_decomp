#include "fft/menu.h"
#include "psx/types.h"

/*
 * Three-pointer variant of text_advance_cursor_with_backreferences
 * (0x80130718): the same text-stream walk, but the caller supplies the
 * remaining-byte counter and the resume cursor separately instead of a state
 * record. `scratch` plays the role of that record's 32-bit scratch value: first the
 * high bits of the byte count, then the back-offset.
 */
u8* text_advance_cursor_with_separate_backreference_state(s32* remaining_bytes, u8* cursor, u8** return_cursor) {
    text_backref_t* backref;
    s32 remaining;
    s32 scratch;
    u8 command;

    remaining = *remaining_bytes;
    cursor += 1;
    if (remaining > 0) {
        remaining -= 1;
        *remaining_bytes = remaining;
        if (remaining == 0) {
            cursor = *return_cursor;
            *return_cursor = (u8*)-1;
        }
    }
    command = cursor[0];
    if (((command & 0xF0) == TEXT_BACKREFERENCE_PREFIX_FIRST) && ((command & 0xF) < 4)) {
        *return_cursor = cursor + 3;
        backref = (text_backref_t*)cursor;
        scratch = backref->length_high << 3;
        *remaining_bytes = scratch + backref->length_low + 4;
        scratch = backref->offset_high;
        scratch = scratch * 0xFE + backref->offset_low;
        cursor -= scratch;
    }
    return cursor;
}
