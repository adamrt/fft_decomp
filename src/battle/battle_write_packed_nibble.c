#include "psx/types.h"

/* Write one nibble into a packed nibble array while preserving its neighbor.
 *
 * Matching note: the parity test lands in the mask's own register,
 * which is dead there because `data` has already been copied out of it, so the
 * parity has to be assigned to `mask` rather than tested inline; a fresh local
 * or an inline `if (!(index & 1))` puts the `andi` in the return register. */
void battle_write_packed_nibble(u8* data, s32 index, s32 row, s32 stride, s32 value) {
    s32 mask;
    u8* destination;

    mask = index & 1;
    if (mask == 0) {
        mask = 0xf;
    } else {
        mask = 0xf0;
    }
    if (mask == 0xf0) {
        value <<= 4;
    }
    destination = data + ((row * stride + index) >> 1);
    *destination = (*destination & ~mask) | value;
}
