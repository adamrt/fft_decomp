#include "fft/wldcore.h"

/* Returns whether bit `index` of the little-endian word array is set.
 *
 * Building the mask before loading the word, and ANDing two locals, gives the
 * target's order: expand swaps two register operands, so the loaded word ends
 * up as the AND's first operand and shares $v0 with the result. */
s32 wldcore_test_bit(u32* bits, s32 index) {
    s32 word = index / 32;
    s32 bit = index & 31;
    u32 mask = 1 << bit;
    u32 value = bits[word];

    return (value & mask) != 0;
}
