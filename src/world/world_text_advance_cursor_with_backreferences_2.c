#include "fft/world.h"
#include "psx/types.h"

/* WORLD twin of battle_text_advance_cursor_with_backreferences (0x80130718). */
u8* world_text_advance_cursor_with_backreferences_2(world_text_backreference_state_t* state, u8* cursor) {
    s32 remaining;
    u8 command;

    remaining = state->remaining_bytes;
    cursor += 1;
    if (remaining > 0) {
        remaining -= 1;
        state->remaining_bytes = remaining;
        if (remaining == 0) {
            cursor = state->return_cursor;
            state->return_cursor = (u8*)-1;
        }
    }
    command = cursor[0];
    /* Commands 0xF0..0xF3 jump the cursor backwards for a fixed number of
     * bytes, then resume after the 3-byte command. */
    if (((command & 0xF0) == 0xF0) && ((command & 0xF) < 4)) {
        state->return_cursor = cursor + 3;
        state->scratch = (cursor[0] & 3) * 8;
        state->remaining_bytes = cursor[1] >> 5;
        state->remaining_bytes = state->remaining_bytes + state->scratch + 4;
        state->scratch = (cursor[1] & 0xF) * 0xFE;
        state->scratch = cursor[2] + state->scratch;
        cursor -= state->scratch;
    }
    return cursor;
}
