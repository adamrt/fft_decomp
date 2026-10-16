/* The SMD and game heap allocators, and file loading to the heap or to an address, checked or not. */

#include "fft/main.h"
#include "fft/main_file.h"
#include "psx/libetc.h"
#include "psx/suzuki.h"
#include "psx/types.h"

static u8 g_main_heap_game_allocator_table[MAIN_HEAP_GAME_PAGE_COUNT] = { 0 };

/* Allocate size bytes from the music heap: 16 pages of 0x800 bytes at
 * g_main_heap_smd_base, tagged in g_main_heap_smd_allocator_table. The algorithm
 * is main_heap_alloc's; returns 0 when no run of free pages is long enough. */
void* main_heap_alloc_smd(u32 size) {
    u32 run = 0;
    u32 needed = size >> MAIN_HEAP_PAGE_SHIFT;
    u32 tag = 1;
    u32 index;
    u32 start;

    if ((size & (MAIN_HEAP_PAGE_SIZE - 1)) != 0) {
        needed++;
    }
    for (index = 0; index < MAIN_HEAP_SMD_PAGE_COUNT; index++) {
        u32 cell;

        cell = g_main_heap_smd_allocator_table[index];
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
            g_main_heap_smd_allocator_table[start + index] = tag;
        }
        return &g_main_heap_smd_base[start << MAIN_HEAP_PAGE_SHIFT];
    }
    return 0;
}

/* Free the music-heap block that starts at ptr; the rules are main_heap_free's. */
s32 main_heap_free_smd(void* ptr) {
    u32 index = ((u32)ptr - (u32)g_main_heap_smd_base) >> MAIN_HEAP_PAGE_SHIFT;
    s32 tag = g_main_heap_smd_allocator_table[index];
    u8* entry;

    if (((index == 0) | (tag != g_main_heap_smd_allocator_table[index - 1])) == 0) {
        return 0;
    }
    entry = &g_main_heap_smd_allocator_table[index];
    do {
        *entry++ = 0;
    } while (*entry == tag);
    return 1;
}

/* Mark every page of the music (SMD) heap free. */
void main_heap_clear_smd_allocator_table(void) {
    int index = MAIN_HEAP_SMD_PAGE_COUNT - 1;
    u8* entry = &g_main_heap_smd_allocator_table[MAIN_HEAP_SMD_PAGE_COUNT - 1];

    for (; index >= 0; index--, entry--) {
        *entry = 0;
    }
}

/* Allocate size bytes from the game heap: 64 pages of 0x800 bytes starting at
 * g_main_heap_high_overlay_load_address, one tag byte per page in
 * g_main_heap_game_allocator_table (0 = free).
 *
 * First fit over whole pages. The new block's tag is one more than the highest
 * tag met before the block, so it differs from the block below it, but the block
 * above can hold the same tag; main_heap_free tells blocks apart by tag alone.
 * Returns 0 when no run of free pages is long enough. */
void* main_heap_alloc(u32 size) {
    u32 run = 0;
    u32 needed = size >> MAIN_HEAP_PAGE_SHIFT;
    u32 tag = 1;
    u32 index;
    u32 start;

    if ((size & (MAIN_HEAP_PAGE_SIZE - 1)) != 0) {
        needed++;
    }
    for (index = 0; index < MAIN_HEAP_GAME_PAGE_COUNT; index++) {
        u32 cell = g_main_heap_game_allocator_table[index];

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
            g_main_heap_game_allocator_table[start + index] = tag;
        }
        return (start << MAIN_HEAP_PAGE_SHIFT) + g_main_heap_high_overlay_load_address;
    }
    return 0;
}

/* Reserve free heap pages at a fixed address, using the lowest unused tag.
 * Only the starting address is bounds-checked; the page range may extend
 * beyond the heap. Returns the page-aligned address, or 0 on failure. */
u8* main_heap_reserve_at(u8* allocation, u32 size) {
    u32 addr = (u32)allocation;
    u32 base = (u32)g_main_heap_high_overlay_load_address;
    s32 start;
    u32 nblocks;
    s32 id = 1;
    u32 i;
    u32 j;

    start = (allocation - g_main_heap_high_overlay_load_address) / MAIN_HEAP_PAGE_SIZE;
    nblocks = size >> MAIN_HEAP_PAGE_SHIFT;
    if ((size & (MAIN_HEAP_PAGE_SIZE - 1)) != 0) {
        nblocks += 1;
    }
    if (addr < base) {
        return 0;
    }
    if (addr < base + MAIN_HEAP_GAME_PAGE_COUNT * MAIN_HEAP_PAGE_SIZE) {
        do {
            for (i = 0; i < MAIN_HEAP_GAME_PAGE_COUNT; i++) {
                if (g_main_heap_game_allocator_table[i] == id) {
                    id += 1;
                    break;
                }
            }
        } while (i != MAIN_HEAP_GAME_PAGE_COUNT);

        for (i = start; i < start + nblocks; i++) {
            if (g_main_heap_game_allocator_table[i] != 0) {
                break;
            }
        }
        if (i == start + nblocks) {
            for (j = 0; j < nblocks; j++) {
                g_main_heap_game_allocator_table[start + j] = id;
            }
            return ((start << MAIN_HEAP_PAGE_SHIFT) + g_main_heap_high_overlay_load_address);
        }
    }
    return 0;
}

/* Free the game-heap block that starts at pointer: clear its page tags from that
 * page on while they repeat the block's tag. Returns 1, or 0 without freeing when
 * the page below holds the same tag (pointer is taken to be inside a block).
 *
 * A block directly above with the same tag (see main_heap_alloc) is freed with
 * it. For page 0 the test also reads the byte before the table. */
s32 main_heap_free(void* pointer) {
    u32 page_index = (u32)((u8*)pointer - g_main_heap_high_overlay_load_address) >> MAIN_HEAP_PAGE_SHIFT;
    s32 allocation_id = g_main_heap_game_allocator_table[page_index];
    s32 previous_id = g_main_heap_game_allocator_table[page_index - 1];

    if ((page_index < 1) | (allocation_id != previous_id)) {
        u8* entry = &g_main_heap_game_allocator_table[page_index];

        do {
            *entry++ = 0;
        } while (*entry == allocation_id);
        return 1;
    }
    return 0;
}

/* Mark every page of the game heap free; startup and each later game reset call it. */
void main_heap_clear_game_allocator_table(void) {
    int index = MAIN_HEAP_GAME_PAGE_COUNT - 1;
    u8* entry = &g_main_heap_game_allocator_table[MAIN_HEAP_GAME_PAGE_COUNT - 1];

    for (; index >= 0; index--, entry--) {
        *entry = 0;
    }
}

/*
 * Start an asynchronous disc read through the shared file header.
 *
 * Overlay callers pass a byte count; the lower-level builder takes sectors.
 */
s32 main_file_call_build_header(s32 sector, u32 size, void* destination) {
    return main_file_request_read_quiet(&g_main_file_cd_state, sector, size >> 11, destination);
}

/* Return the shared CD descriptor's load state: nonzero while a read through g_main_file_cd_state
 * is in progress. */
int main_file_is_still_loading(void) {
    return g_main_file_still_loading;
}

/* Blocking read of a sound file into a new block of the music heap. size is in
 * bytes and only whole sectors are read. A full heap reports
 * MAIN_SYSTEM_ERROR_SMD_LOAD and returns 0; the loader then only seeks, because
 * it skips the read for a null destination. */
suzuki_smd_header_t* main_file_get_smd(int sector, u32 size) {
    suzuki_smd_header_t* buffer = main_heap_alloc_smd(size);
    main_file_load_descriptor_t* header;

    if (buffer == 0) {
        main_system_handle_error(MAIN_SYSTEM_ERROR_SMD_LOAD);
    }
    while (main_file_request_read_quiet(&g_main_file_cd_state, sector, size >> 11, buffer) != 0) {
        VSync(0);
        main_system_frame_stub();
        main_file_poll_load(&g_main_file_cd_state);
    }
    if (g_main_file_still_loading != 0) {
        header = &g_main_file_cd_state;
        do {
            VSync(0);
            main_system_frame_stub();
            main_file_poll_load(header);
        } while (g_main_file_still_loading != 0);
    }
    return buffer;
}

/* Allocates a game-heap buffer for a file of `size` bytes at disc `sector`,
 * queues the read and blocks until the file has landed.  Twin of
 * main_file_get_smd, which draws from the SMD allocator instead. */
static void* main_file_load_to_heap(s32 sector, u32 size) {
    void* destination = main_heap_alloc(size);
    main_file_load_descriptor_t* header;

    if (destination == 0) {
        main_system_handle_error(MAIN_SYSTEM_ERROR_HEAP_ALLOC);
    }
    while (main_file_request_read_quiet(&g_main_file_cd_state, sector, (size >> 11), destination) != 0) {
        VSync(0);
        main_system_frame_stub();
        main_file_poll_load(&g_main_file_cd_state);
    }
    if (g_main_file_still_loading != 0) {
        header = &g_main_file_cd_state;
        do {
            VSync(0);
            main_system_frame_stub();
            main_file_poll_load(header);
        } while (g_main_file_still_loading != 0);
    }
    return destination;
}

/* Blocking read from a disc sector to destination without the "Now Loading"
 * message. size is in bytes and only whole sectors (size >> 11) are read. Waits
 * for the shared loader to be idle, then polls it once per frame until the read
 * ends. Always returns destination. */
void* main_file_load_to_address(int sector, unsigned int size, void* destination) {
    main_file_load_descriptor_t* header;

    while (main_file_request_read_quiet(&g_main_file_cd_state, sector, size >> 11, destination) != 0) {
        VSync(0);
        main_system_frame_stub();
        main_file_poll_load(&g_main_file_cd_state);
    }
    if (g_main_file_still_loading != 0) {
        header = &g_main_file_cd_state;
        do {
            VSync(0);
            main_system_frame_stub();
            main_file_poll_load(header);
        } while (g_main_file_still_loading != 0);
    }
    return destination;
}

/* Blocking main_file_load_to_address that reports MAIN_SYSTEM_ERROR_FILE_LOAD on a null result;
 * since that loader returns destination, only a null destination trips it. */
void* main_file_load_checked_to_address(int sector, int size, void* destination) {
    void* result = main_file_load_to_address(sector, size, destination);

    if (result == 0) {
        main_system_handle_error(MAIN_SYSTEM_ERROR_FILE_LOAD);
    }
    return result;
}

/* Blocking load into a new game-heap block that also reports MAIN_SYSTEM_ERROR_FILE_LOAD when no
 * block was free (main_file_load_to_heap has already reported MAIN_SYSTEM_ERROR_HEAP_ALLOC). */
void* main_file_load_checked_to_heap(int sector, int size) {
    void* result = main_file_load_to_heap(sector, size);

    if (result == 0) {
        main_system_handle_error(MAIN_SYSTEM_ERROR_FILE_LOAD);
    }
    return result;
}

/* Free a game-heap block through main_heap_free, dropping its result. */
void main_heap_call_free(void* pointer) {
    main_heap_free(pointer);
}
