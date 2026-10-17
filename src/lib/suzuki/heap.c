/* Sound-engine CPU heap allocation and block list. */

#include "fft/suzuki.h"

/* Suzuki heap block header (0x10 bytes; payload follows). The $gp-relative
 * allocator (0x8001423c-0x8001442c) rounds requests to 16 bytes and chains
 * blocks in address order from g_suzuki_sound_heap_blocks. */
typedef struct suzuki_heap_block {
    u16 flags;                      /* 0x00; suzuki_heap_block_flag_e; written, never tested */
    u16 _unknown_02;                /* 0x02; cleared on allocation, never read */
    u32 _unknown_04;                /* 0x04; cleared on allocation, never read */
    u8* end;                        /* 0x08; end of the payload */
    struct suzuki_heap_block* next; /* 0x0c */
} suzuki_heap_block_t;
typedef char suzuki_heap_block_size_must_be_16[sizeof(suzuki_heap_block_t) == 16 ? 1 : -1];

/* suzuki_heap_block_t.flags. */
typedef enum suzuki_heap_block_flag {
    SUZUKI_HEAP_BLOCK_ALLOCATED = 0x0002,
    SUZUKI_HEAP_BLOCK_HEAD = 0x8000 /* the list head suzuki_sound_init_heap places at the arena start */
} suzuki_heap_block_flag_e;

extern suzuki_heap_block_t* g_suzuki_sound_heap_arena;
extern suzuki_heap_block_t* g_suzuki_sound_heap_blocks;
extern u32 g_suzuki_sound_heap_size;
extern u8* g_suzuki_sound_heap_end;

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

/* Unlink the heap block that owns payload from the driver's block list; its space becomes a gap
 * for suzuki_sound_alloc. The block must be in the list. */
void suzuki_sound_free(void* payload) {
    suzuki_heap_block_t* block = (suzuki_heap_block_t*)payload - 1;
    suzuki_heap_block_t* current = g_suzuki_sound_heap_blocks;
    suzuki_heap_block_t* previous = 0;
    while (current != block) {
        previous = current;
        current = previous->next;
    }
    if (previous != 0) {
        previous->next = block->next;
    } else {
        g_suzuki_sound_heap_blocks = g_suzuki_sound_heap_blocks->next;
    }
}

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
