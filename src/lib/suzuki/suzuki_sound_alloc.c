/* SCUS_942.21 0x80014278..0x80014357. */
#include "psx/suzuki.h"

/* First fit in the driver heap: find the first gap after a block (before the next block or the
 * heap end) that holds a 16-byte header plus size rounded up to 16, link a new block there and
 * return its zeroed payload; 0 when nothing fits. */
void* suzuki_sound_alloc(u32 size) {
    suzuki_heap_block_t* block = g_suzuki_sound_heap_blocks;
    suzuki_heap_block_t* allocated;
    u8* payload;
    u8* limit;
    u32 required = ((size + 15) & ~15) + sizeof(suzuki_heap_block_t);
    while (block->next != 0) {
        limit = (u8*)block->next;
        if ((limit - block->end) >= required) {
            goto found;
        }
        block = block->next;
    }
    limit = g_suzuki_sound_heap_end;
    if ((limit - block->end) >= required) {
    found:
        allocated = (suzuki_heap_block_t*)(((u32)block->end + 15) & ~15);
        payload = (u8*)(allocated + 1);
        allocated->end = payload + size;
        allocated->next = 0;
        allocated->_unknown_04 = 0;
        allocated->flags = SUZUKI_HEAP_BLOCK_ALLOCATED;
        allocated->_unknown_02 = 0;
        allocated->next = block->next;
        block->next = allocated;
        suzuki_sound_clear_memory(payload, size);
        return payload;
    }
    return 0;
}
