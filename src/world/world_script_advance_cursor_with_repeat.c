#include "psx/types.h"

/* `nibble` carries the opcode test and then the displacement nibble: sharing one
 * variable across the two blocks makes it a global allocno, so local_alloc
 * cannot tie the masked value to the dying byte load.  `value` likewise holds the
 * product, which keeps the final addu's result in the displacement register. */

/* Three-byte repeat opcode (0xF0..0xF3) in a byte-coded script stream. */
typedef struct {
    u8 count_hi : 2;
    u8 : 2;
    u8 opcode : 4;
    u8 disp_hi : 4;
    u8 : 1;
    u8 count_lo : 3;
    u8 disp_lo;
} world_script_repeat_t;

/* Advance a byte-coded script cursor by one byte, honouring a pending
 * repeat counter (*count) whose resume address is *saved.  A repeat opcode
 * at the new position starts a new repeat: it stores the resume address,
 * derives the repeat count and rewinds by the packed displacement. */
u8* world_script_advance_cursor_with_repeat(s32* count, u8* cursor, u8** saved) {
    s32 value = *count;
    s32 nibble;
    world_script_repeat_t* op;

    cursor += 1;
    if (value > 0) {
        value -= 1;
        *count = value;
        if (value == 0) {
            cursor = *saved;
            *saved = (u8*)-1;
        }
    }
    op = (world_script_repeat_t*)cursor;
    nibble = cursor[0] & 0xF0;
    if (nibble == 0xF0 && (cursor[0] & 0xF) < 4) {
        *saved = cursor + 3;
        value = (op->count_hi << 3) + op->count_lo;
        *count = value + 4;
        nibble = op->disp_hi;
        value = nibble * 254;
        cursor -= value + op->disp_lo;
    }
    return cursor;
}
