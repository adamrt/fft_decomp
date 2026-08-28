#include "psx/types.h"

/* Set or clear one bit in a word-indexed bitmap. */
void world_set_bitmap_bit(u32* bits, s32 index, s32 set) {
    s32 word = index / 32;
    u32 mask = 1 << (index & 0x1F);

    if (set != 0) {
        bits[word] |= mask;
    } else {
        bits[word] &= ~mask;
    }
}
