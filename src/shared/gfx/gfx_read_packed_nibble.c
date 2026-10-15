#include "fft/battle_text.h"
#include "psx/types.h"

/* Reads one nibble out of a packed nibble array: odd indices take the high
 * nibble, even indices the low one. */
s32 gfx_read_packed_nibble(const u8* data, s32 index, s32 row, s32 stride) {
    s32 mask;
    s32 value;

    mask = index & 1;
    if (mask == 0) {
        mask = 0xF;
    } else {
        mask = 0xF0;
    }
    value = data[(row * stride + index) >> 1] & mask;
    if (mask == 0xF0) {
        value >>= 4;
    }
    return value;
}
