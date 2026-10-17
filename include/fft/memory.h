#ifndef MEMORY_H
#define MEMORY_H

/* Memory utilities: MAIN's byte copies and page heaps, the event library's forward copy, ATTACK's unaligned load, and
 * MAIN's random-roll and vector setters. */

#include "psx/libgte.h"
#include "psx/types.h"

/* byte copies */
s16 mem_load_unaligned_s16(const u8* ptr);

/* Source-first byte copies; nonpositive counts leave the destination unchanged. */
void mem_copy_bytes_source_first(const void* source, void* destination, int count);
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
    HEAP_SMD_PAGE_COUNT = 16,
};

extern u32 g_heap_battle_end_address;
extern u8* g_heap_high_overlay_load_address;
extern u8* g_heap_low_overlay_load_address;
extern u8* g_heap_world_overlay_load_address;
extern u8* g_heap_companion_overlay_load_address;

void* heap_alloc(u32 size);
s32 heap_free(void* allocation);
void heap_call_free(void* allocation);
u8* heap_reserve_at(u8* allocation, u32 size);
void heap_clear_game_table(void);
void* heap_alloc_smd(u32 size);
s32 heap_free_smd(void* allocation);
void heap_clear_smd_table(void);

/* random values and vectors */
/* A random value between `min` and `max`, in either order. Spelled with rand()
 * first, unlike RAND_BETWEEN; both spellings occur in the target. */
#define RANDOM_BETWEEN(min, max)                                                                                       \
    ((min) == (max) ? (min) : (max) < (min) ? rand() % ((min) - (max)) + (max) : rand() % ((max) - (min)) + (min))

s32 random_roll_pass_fail(s32 maximum, s32 threshold);
void vector_set_svector(SVECTOR* vector, s32 x, s32 y, s32 z);
void vector_set_vector(VECTOR* vector, s32 x, s32 y, s32 z);

#endif
