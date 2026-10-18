/* First-fit allocation of menu image buffers from the program's page arena. */

#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/system.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* A first-fit allocator over 256-byte pages with sixteen allocation slots. Each program has its own arena of
 * VARIANT_MENU_BUFFER_PAGE_COUNT pages (53 in BATTLE, 112 in WORLD); menu_alloc_memory yields until a run of
 * free pages and a free slot exist. */
enum { MENU_BUFFER_ALLOCATION_COUNT = 16, MENU_BUFFER_PAGE_SIZE = 256 };

/* One allocation: its first-page pointer (-1 for a free slot) and allocated page count. */
typedef struct menu_allocation {
    u8* buffer;     /* 0x00 */
    s32 page_count; /* 0x04 */
} menu_allocation_t;
typedef char menu_allocation_size_must_be_8[(sizeof(menu_allocation_t) == 8) ? 1 : -1];

static menu_allocation_t g_menu_allocator_slot_records[MENU_BUFFER_ALLOCATION_COUNT];

/* WORLD's g_ai_workspace pointer cell aliases bytes inside this arena. */
static u8 g_menu_buffer_arena[VARIANT_MENU_BUFFER_PAGE_COUNT][MENU_BUFFER_PAGE_SIZE];

/* Nonzero for each arena page in use. */
static u8 g_menu_buffer_allocation_map[VARIANT_MENU_BUFFER_PAGE_COUNT];

/* Reset the menu arena allocator: every allocation slot unused (-1), every arena page free, and
 * the three typewriter glyph slots unowned. */
void menu_init_buffer_allocator(void) {
    s32 fill;
    s32 slot;
    s32 count;
    u8* byte;
    s32* first;
    s32* second;

    /* One counter local shared by the last two loops and each counter set
     * before its pointer: this reproduces the target's register assignment
     * without pins. */
    fill = -1;
    for (slot = MENU_BUFFER_ALLOCATION_COUNT - 1; slot >= 0; slot--) {
        g_menu_allocator_slot_records[slot].buffer = (u8*)fill;
    }
    count = VARIANT_MENU_BUFFER_PAGE_COUNT - 1;
    byte = &g_menu_buffer_allocation_map[VARIANT_MENU_BUFFER_PAGE_COUNT - 1];
    for (; count >= 0; count--) {
        *byte = 0;
        byte--;
    }
    count = 0;
    second = g_text_slot_glyph_counts;
    first = g_text_slot_owner_thread_ids;
    for (; count < 3; count++) {
        *first = 0;
        *second = 0;
        second++;
        first++;
    }
}

/* Claims enough 256-byte arena pages for the requested bytes and records them in a free slot.
 *
 * Scans the page occupancy map first-fit. When no run of free pages or no free slot
 * remains, it optionally prints a diagnostic, yields the thread, and rescans.
 * The arena holds VARIANT_MENU_BUFFER_PAGE_COUNT pages (53 in BATTLE, 112 in WORLD).
 * Retail checks the arena end before accepting a run, so positive allocations never use the final page.
 * If all slots are occupied, the newly marked run remains without an owner across retries. */
void* menu_alloc_memory(s32 size) {
    s32 first_page;
    s32 run_pages;
    s32 slot;
    u8* buffer;

    /* Reuse size as the page count; a separate local changes the original code. */
    size = (size & 0xFF) != 0 ? (size >> 8) + 1 : size >> 8;
    for (;;) {
        for (first_page = 0; first_page < VARIANT_MENU_BUFFER_PAGE_COUNT; first_page++) {
            if (g_menu_buffer_allocation_map[first_page] == 0) {
                for (run_pages = 0;; run_pages++) {
                    if (run_pages + first_page >= VARIANT_MENU_BUFFER_PAGE_COUNT) {
                        /* Leaves the run and page loops for the retry wait. */
                        goto fail;
                    }
                    if (run_pages >= size || g_menu_buffer_allocation_map[run_pages + first_page] != 0) {
                        break;
                    }
                }
                if (run_pages >= size) {
                    for (run_pages = 0; run_pages < size; run_pages++) {
                        g_menu_buffer_allocation_map[first_page + run_pages] = 1;
                    }
                    for (slot = 0; slot < MENU_BUFFER_ALLOCATION_COUNT; slot++) {
                        if (g_menu_allocator_slot_records[slot].buffer == (u8*)-1) {
                            buffer = g_menu_buffer_arena[first_page];
                            g_menu_allocator_slot_records[slot].buffer = buffer;
                            g_menu_allocator_slot_records[slot].page_count = run_pages;
                            return buffer;
                        }
                    }
                    break;
                }
            }
        }
    fail:
        if (g_debug_font_print_enabled != 0) {
            FntPrint("WAIT FOR ALLOCATION \nIN TASK%d %d\n", g_thread_current_id, size);
        }
        thread_yield();
    }
}

/* Public wrapper for the menu-buffer allocator. */
void* menu_alloc_buffer(s32 bytes) {
    return menu_alloc_memory(bytes);
}

/* Free a menu arena allocation: release its slot record and clear its pages in the map.
 * Pointers at or above the companion overlay load address, and -1, are ignored; any other
 * pointer that no slot records stops the current thread. */
void menu_free_memory(void* buffer_pointer) {
    u32 buffer = (u32)buffer_pointer;
    u32 first_page;
    s32* page_count_cursor;
    menu_allocation_t* allocation;
    s32* page_count_end;
    u8* allocation_grid;
    s32 invalid_buffer;
    s32 page;

    if (buffer < (u32)g_heap_companion_overlay_load_address && buffer != -1) {
        invalid_buffer = -1;
        first_page = (buffer - (u32)g_menu_buffer_arena) >> 8;
        allocation_grid = g_menu_buffer_allocation_map;
        /* A separate page-count cursor preserves the native count-field base. */
        page_count_cursor = &g_menu_allocator_slot_records[0].page_count;
        allocation = g_menu_allocator_slot_records;
        page_count_end = &g_menu_allocator_slot_records[MENU_BUFFER_ALLOCATION_COUNT].page_count;
        for (;;) {
            if ((u32)allocation->buffer == buffer) {
                allocation->buffer = (u8*)invalid_buffer;
                /* Written as (index + base) + grid so the two addu operands
                 * come out in the target's order; the page count is reloaded
                 * each iteration because the byte store may alias it. */
                for (page = 0; page < *page_count_cursor; page++) {
                    *(u8*)((page + first_page) + (u32)allocation_grid) = 0;
                }
                return;
            }
            page_count_cursor += sizeof(menu_allocation_t) / sizeof(*page_count_cursor);
            allocation++;
            if ((s32)page_count_cursor >= (s32)page_count_end) {
                thread_exit_current();
                return;
            }
        }
    }
}
