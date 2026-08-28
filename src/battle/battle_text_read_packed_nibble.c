#include "psx/types.h"

/* Reads one nibble out of a packed nibble array: odd indices take the high
 * nibble, even indices the low one. */
s32 battle_text_read_packed_nibble(u8* data, s32 index, s32 row, s32 stride) {
    u8* base = data;
    s32 mask;
    s32 value;

    mask = index & 1;
    if (mask == 0) {
        mask = 0xF;
    } else {
        mask = 0xF0;
    }
    value = *(base + ((row * stride + index) >> 1)) & mask;
    if (mask == 0xF0) {
        value >>= 4;
    }
    return value;
}
