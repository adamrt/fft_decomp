#include "psx/types.h"

/*
 * The back-offset is built in its own `offset` local in three steps, which
 * gives the target's v0/v1 split for the cursor[1] chain.
 *
 * Three-pointer variant of battle_text_advance_cursor_with_backreferences
 * (0x80130718): the same text-stream walk, but the caller supplies the
 * remaining-byte counter and the resume cursor separately instead of a state
 * record.
 */
u8* battle_text_advance_cursor_with_separate_backreference_state(s32* remaining_bytes, u8* cursor, u8** return_cursor) {
    s32 remaining;
    s32 scratch;
    s32 offset;
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
    if (((command & 0xF0) == 0xF0) && ((command & 0xF) < 4)) {
        *return_cursor = cursor + 3;
        scratch = (cursor[0] & 3) * 8;
        *remaining_bytes = scratch + (cursor[1] >> 5) + 4;
        /* Keeps scratch live past the sum; local-alloc otherwise ties the sum to it and swaps $v0/$v1. */
        __asm__("" : : "r"(scratch));
        offset = cursor[1] & 0xF;
        offset = offset * 0xFE;
        offset = cursor[2] + offset;
        cursor -= offset;
    }
    return cursor;
}
