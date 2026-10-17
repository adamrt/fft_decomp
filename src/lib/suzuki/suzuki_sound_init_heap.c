/* SCUS_942.21 0x8001423c..0x80014277. */
#include "fft/suzuki.h"

/* Set up the driver's CPU heap over arena: size is rounded down to 16 bytes, and an empty
 * SUZUKI_HEAP_BLOCK_HEAD block at the arena start begins the block list. */
void suzuki_sound_init_heap(void* arena, u32 size) {
    suzuki_heap_block_t* head = arena;
    size &= ~15;
    g_suzuki_sound_heap_end = (u8*)arena + size;
    head->flags = SUZUKI_HEAP_BLOCK_HEAD;
    g_suzuki_sound_heap_arena = head;
    g_suzuki_sound_heap_size = size;
    g_suzuki_sound_heap_blocks = head;
    head->_unknown_02 = 0;
    head->_unknown_04 = 0;
    head->end = (u8*)(head + 1);
    head->next = 0;
}
