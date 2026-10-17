/* SCUS_942.21 0x800143ac..0x8001442b. */
#include "fft/suzuki.h"

/* Reports the aligned gap, including space required for the next allocation record. */
s32 suzuki_sound_get_largest_free_block(void) {
    suzuki_heap_block_t* block = g_suzuki_sound_heap_blocks;
    s32 largest = 0;
    s32 gap;
    while (block->next != 0) {
        gap = ((u8*)block->next - block->end) & ~15;
        if (gap > largest) {
            largest = gap;
        }
        block = block->next;
    }
    gap = (g_suzuki_sound_heap_end - block->end) & ~15;
    if (gap > largest) {
        largest = gap;
    }
    return largest;
}
