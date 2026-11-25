#ifndef MEMORY_H
#define MEMORY_H

/* Byte copies, word fills, unaligned reads and the page heaps for game and music data. */

#include "psx/types.h"

/* byte copies and reads */
/* A word zero-fill with no C source: hand-written MIPS, kept as original bytes through the `regions:` rows of
 * target/battle.yaml and target/world.yaml. */
void mem_zero_words(void* buffer, u32 bytes);

s16 mem_load_unaligned_s16(const u8* ptr);

/* Source-first byte copies; nonpositive counts leave the destination unchanged. */
void mem_copy_byte_data_source_first(const void* source, void* destination, s32 count);

/* Clear count bytes; nonpositive counts leave the destination unchanged. */
void mem_zero(void* destination, s32 count);

/* The event library's forward copy for BATTLE and WORLD, which the menu library and both programs also call; count
 * must be positive. */
void mem_copy_bytes_nonempty(void* destination, const void* source, s32 count);

/* heaps */
/* Game and music heaps allocate whole pages, tagged by these byte tables. */
enum {
    HEAP_PAGE_SHIFT = 11,
    HEAP_PAGE_SIZE = 1 << HEAP_PAGE_SHIFT,
    HEAP_GAME_PAGE_COUNT = 64,
    HEAP_SMD_PAGE_COUNT = 16
};

extern u8* g_heap_high_overlay_load_address;
extern u8* g_heap_low_overlay_load_address;
extern u8* g_heap_companion_overlay_load_address;

void* heap_alloc(u32 size);
s32 heap_free(void* allocation);
void heap_release_block(void* allocation);
void* heap_reserve_at(void* allocation, u32 size);
void heap_clear_game_table(void);
void* heap_smd_alloc(u32 size);
s32 heap_smd_free(void* allocation);
void heap_smd_clear_table(void);

#endif
