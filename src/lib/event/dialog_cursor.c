/* The dialog text cursor: advancing it with separate backreference state and printing FNT values. */

#include "fft/text.h"
#include "psx/libgpu.h"
#include "psx/types.h"

extern const char g_text_decimal_format[];

/* text_advance_cursor_with_backreferences with the remaining run length and return cursor passed
 * separately instead of in a text_reader_state_t. */
const u8* text_advance_cursor_with_separate_backreference_state(
    s32* remaining_bytes, const u8* cursor, const u8** return_cursor) {
    const text_backref_t* backref;
    s32 remaining = *remaining_bytes;
    s32 scratch;
    u8 command;

    cursor += 1;
    if (remaining > 0) {
        remaining -= 1;
        *remaining_bytes = remaining;
        if (remaining == 0) {
            cursor = *return_cursor;
            *return_cursor = (const u8*)-1;
        }
    }
    command = cursor[0];
    if (((command & 0xF0) == TEXT_BACKREFERENCE_PREFIX_FIRST) && ((command & 0xF) < 4)) {
        *return_cursor = cursor + 3;
        backref = (const text_backref_t*)cursor;
        scratch = backref->length_high << 3;
        *remaining_bytes = scratch + backref->length_low + 4;
        scratch = backref->offset_high;
        scratch = scratch * 0xFE + backref->offset_low;
        cursor -= scratch;
    }
    return cursor;
}

/* Append value and value + 1 to the active Psy-Q font stream. */
void text_print_fnt_value_and_next(s32 value) {
    FntPrint(g_text_decimal_format, value);
    value++;
    FntPrint(g_text_decimal_format, value);
}
