/* Resident game and music heap allocation. */

#include "fft/memory.h"
#include "psx/types.h"

static u8 g_heap_game_allocator_table[HEAP_GAME_PAGE_COUNT] = { 0 };
extern u8 g_heap_smd_allocator_table[HEAP_SMD_PAGE_COUNT];
extern u8 g_heap_smd_base[HEAP_SMD_PAGE_COUNT * HEAP_PAGE_SIZE];

/* Allocate size bytes from the music heap: 16 pages of 0x800 bytes at
 * g_heap_smd_base, tagged in g_heap_smd_allocator_table. The algorithm
 * is heap_alloc's; returns 0 when no run of free pages is long enough. */
void* heap_smd_alloc(u32 size) {
    u32 run = 0;
    u32 needed = size >> HEAP_PAGE_SHIFT;
    u32 tag = 1;
    u32 index;
    u32 start;

    if ((size & (HEAP_PAGE_SIZE - 1)) != 0) {
        needed++;
    }
    for (index = 0; index < HEAP_SMD_PAGE_COUNT; index++) {
        u32 cell;

        cell = g_heap_smd_allocator_table[index];
        if (cell == 0) {
            if (run == 0) {
                start = index;
            }
            run++;
            if (run >= needed) {
                break;
            }
        } else {
            if (cell >= tag) {
                tag = cell + 1;
            }
            run = 0;
        }
    }
    if (run >= needed) {
        for (index = 0; index < run; index++) {
            g_heap_smd_allocator_table[start + index] = tag;
        }
        return &g_heap_smd_base[start << HEAP_PAGE_SHIFT];
    }
    return 0;
}

/* Free the music-heap block that starts at ptr; the rules are heap_free's. */
s32 heap_smd_free(void* ptr) {
    u32 index = (u32)((u8*)ptr - g_heap_smd_base) >> HEAP_PAGE_SHIFT;
    s32 tag = g_heap_smd_allocator_table[index];
    u8* entry;

    if (((index == 0) | (tag != g_heap_smd_allocator_table[index - 1])) == 0) {
        return 0;
    }
    entry = &g_heap_smd_allocator_table[index];
    do {
        *entry++ = 0;
    } while (*entry == tag);
    return 1;
}

/* Mark every page of the music (SMD) heap free. */
void heap_smd_table_clear(void) {
    int index = HEAP_SMD_PAGE_COUNT - 1;
    u8* entry = &g_heap_smd_allocator_table[HEAP_SMD_PAGE_COUNT - 1];

    for (; index >= 0; index--, entry--) {
        *entry = 0;
    }
}

/* Allocate size bytes from the game heap: 64 pages of 0x800 bytes starting at
 * g_heap_high_overlay_load_address, one tag byte per page in
 * g_heap_game_allocator_table (0 = free).
 *
 * First fit over whole pages. The new block's tag is one more than the highest
 * tag met before the block, so it differs from the block below it, but the block
 * above can hold the same tag; heap_free tells blocks apart by tag alone.
 * Returns 0 when no run of free pages is long enough. */
void* heap_alloc(u32 size) {
    u32 run = 0;
    u32 needed = size >> HEAP_PAGE_SHIFT;
    u32 tag = 1;
    u32 index;
    u32 start;

    if ((size & (HEAP_PAGE_SIZE - 1)) != 0) {
        needed++;
    }
    for (index = 0; index < HEAP_GAME_PAGE_COUNT; index++) {
        u32 cell = g_heap_game_allocator_table[index];

        if (cell == 0) {
            if (run == 0) {
                start = index;
            }
            run++;
            if (run >= needed) {
                break;
            }
        } else {
            if (cell >= tag) {
                tag = cell + 1;
            }
            run = 0;
        }
    }
    if (run >= needed) {
        for (index = 0; index < run; index++) {
            g_heap_game_allocator_table[start + index] = tag;
        }
        return (start << HEAP_PAGE_SHIFT) + g_heap_high_overlay_load_address;
    }
    return 0;
}

/* Reserve free heap pages at a fixed address, using the lowest unused tag.
 * Only the starting address is bounds-checked; the page range may extend
 * beyond the heap. Returns the page-aligned address, or 0 on failure. */
void* heap_reserve_at(void* allocation, u32 size) {
    u32 addr = (u32)allocation;
    u32 base = (u32)g_heap_high_overlay_load_address;
    s32 start;
    u32 nblocks;
    s32 id = 1;
    u32 i;
    u32 j;

    start = ((u8*)allocation - g_heap_high_overlay_load_address) / HEAP_PAGE_SIZE;
    nblocks = size >> HEAP_PAGE_SHIFT;
    if ((size & (HEAP_PAGE_SIZE - 1)) != 0) {
        nblocks += 1;
    }
    if (addr < base) {
        return 0;
    }
    if (addr < base + HEAP_GAME_PAGE_COUNT * HEAP_PAGE_SIZE) {
        do {
            for (i = 0; i < HEAP_GAME_PAGE_COUNT; i++) {
                if (g_heap_game_allocator_table[i] == id) {
                    id += 1;
                    break;
                }
            }
        } while (i != HEAP_GAME_PAGE_COUNT);

        for (i = start; i < start + nblocks; i++) {
            if (g_heap_game_allocator_table[i] != 0) {
                break;
            }
        }
        if (i == start + nblocks) {
            for (j = 0; j < nblocks; j++) {
                g_heap_game_allocator_table[start + j] = id;
            }
            return ((start << HEAP_PAGE_SHIFT) + g_heap_high_overlay_load_address);
        }
    }
    return 0;
}

/* Free the game-heap block that starts at pointer: clear its page tags from that
 * page on while they repeat the block's tag. Returns 1, or 0 without freeing when
 * the page below holds the same tag (pointer is taken to be inside a block).
 *
 * A block directly above with the same tag (see heap_alloc) is freed with
 * it. For page 0 the test also reads the byte before the table. */
s32 heap_free(void* pointer) {
    u32 page_index = (u32)((u8*)pointer - g_heap_high_overlay_load_address) >> HEAP_PAGE_SHIFT;
    s32 allocation_id = g_heap_game_allocator_table[page_index];
    s32 previous_id = g_heap_game_allocator_table[page_index - 1];

    if ((page_index < 1) | (allocation_id != previous_id)) {
        u8* entry = &g_heap_game_allocator_table[page_index];

        do {
            *entry++ = 0;
        } while (*entry == allocation_id);
        return 1;
    }
    return 0;
}

/* Mark every page of the game heap free; startup and each later game reset call it. */
void heap_game_table_clear(void) {
    int index = HEAP_GAME_PAGE_COUNT - 1;
    u8* entry = &g_heap_game_allocator_table[HEAP_GAME_PAGE_COUNT - 1];

    for (; index >= 0; index--, entry--) {
        *entry = 0;
    }
}
