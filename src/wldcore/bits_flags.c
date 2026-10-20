/* World-map save records address flags as bits in arrays of 32-bit words. */

#include "fft/bits.h"
#include "psx/types.h"

/* Set (nonzero `set`) or clear bit `index` of a 32-bit word array. */
void bits_value_set(u32* bits, s32 index, s32 set) {
    u32* base = bits;
    u32 mask;
    s32 word_index = index / 32;

    mask = 1U << (index & 31);
    if (set != 0) {
        base[word_index] |= mask;
    } else {
        base[word_index] &= ~mask;
    }
}

/* Returns whether bit `index` of the little-endian 32-bit array is set.
 *
 * Building the mask before loading the 32-bit value, and ANDing two locals, gives the
 * target's order: expand swaps two register operands, so the loaded 32-bit value ends
 * up as the AND's first operand and shares $v0 with the result. */
s32 bits_test(u32* bits, s32 index) {
    s32 word = index / 32;
    s32 bit = index & 31;
    u32 mask = 1U << bit;
    u32 value = bits[word];

    return (value & mask) != 0;
}
