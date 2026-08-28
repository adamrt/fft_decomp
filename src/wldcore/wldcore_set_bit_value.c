#include "psx/types.h"

/* Assign one bit without changing the rest of its 32-bit storage word.
 *
 * The explicit address expression preserves the target's addu operand order
 * in both branches; typed indexing reverses both operand fields.
 */
void wldcore_set_bit_value(u32* bits, s32 index, s32 set) {
    u32* base;
    u32* clear_word;
    u32* set_word;
    u32 clear_value;
    u32 inverted_mask;
    u32 set_value;
    u32 target_mask;
    u32 one;
    s32 shift;
    s32 word_index;
    s32 adjusted_index;

    base = bits;

    adjusted_index = index;
    if (index < 0) {
        adjusted_index = index + 31;
    }
    word_index = adjusted_index >> 5;
    shift = index & 31;
    one = 1;
    target_mask = one << shift;

    if (set != 0) {
        set_word = (u32*)(word_index * 4 + (s32)base);
        set_value = *set_word;
        set_value = set_value | target_mask;
        *set_word = set_value;
    } else {
        clear_word = (u32*)(word_index * 4 + (s32)base);
        clear_value = *clear_word;
        inverted_mask = ~target_mask;
        clear_value = clear_value & inverted_mask;
        *clear_word = clear_value;
    }
}
