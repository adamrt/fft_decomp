#include "fft/battle.h"

/* Set or clear one indexed flag in a packed 32-bit bitset. */
void battle_script_assign_bitset_flag(u32* bitset, s32 bit_index, s32 value) {
    s32 word_index;
    u32 mask;

    word_index = bit_index / 32;
    mask = 1 << (bit_index & 0x1f);
    if (value != 0) {
        bitset[word_index] |= mask;
    } else {
        bitset[word_index] &= ~mask;
    }
}
