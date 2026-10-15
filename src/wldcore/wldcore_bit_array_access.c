#include "fft/wldcore.h"

/* Assign one bit without changing the rest of its 32-bit storage value. */
void wldcore_set_bit_value(u32* bits, s32 index, s32 set) {
    u32* base;
    u32 mask;
    s32 word_index;

    base = bits;
    word_index = index / 32;
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
s32 wldcore_test_bit(u32* bits, s32 index) {
    s32 word = index / 32;
    s32 bit = index & 31;
    u32 mask = 1U << bit;
    u32 value = bits[word];

    return (value & mask) != 0;
}

/* Count the set bits among the first 0x1000 saved-data bits and return that
 * count as a percentage of 76. */
s32 wldcore_script_calculate_save_data_completion_percent(void) {
    s32 count = 0;
    s32 i;

    for (i = 0; i < 0x1000; i++) {
        if (wldcore_test_bit(g_main_saved_data_bits, i) != 0) {
            count++;
        }
    }
    return count * 100 / 76;
}
