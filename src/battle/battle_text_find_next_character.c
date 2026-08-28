#include "fft/world.h"
#include "psx/types.h"

/* Advance a text cursor by one byte, entering and leaving 0xF0-0xF3
 * back-reference runs. */
u8* battle_text_find_next_character(world_text_backreference_state_t* reader, u8* cursor) {
    s32 count;
    u8 character;

    count = reader->remaining_bytes;
    cursor += 1;
    if (count > 0) {
        count -= 1;
        reader->remaining_bytes = count;
        if (count == 0) {
            cursor = reader->return_cursor;
            reader->return_cursor = (u8*)-1;
        }
    }
    character = cursor[0];
    if (((character & 0xF0) == 0xF0) && ((character & 0xF) < 4)) {
        reader->return_cursor = cursor + 3;
        reader->scratch = (cursor[0] & 3) * 8;
        reader->remaining_bytes = cursor[1] >> 5;
        reader->remaining_bytes = reader->remaining_bytes + reader->scratch + 4;
        reader->scratch = (cursor[1] & 0xF) * 254;
        reader->scratch = cursor[2] + reader->scratch;
        cursor -= reader->scratch;
    }
    return cursor;
}
