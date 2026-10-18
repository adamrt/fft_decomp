/* First-fit allocation of menu image buffers from the program's page arena. */

#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/system.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* The debug line menu_alloc_memory prints while it waits for free pages. */
extern char g_menu_buffer_wait_message[];

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

extern u8 g_menu_buffer_arena[VARIANT_MENU_BUFFER_PAGE_COUNT][MENU_BUFFER_PAGE_SIZE];
extern menu_allocation_t g_menu_allocator_slot_records[MENU_BUFFER_ALLOCATION_COUNT];

/* Nonzero for each arena page in use. */
extern u8 g_menu_buffer_allocation_map[VARIANT_MENU_BUFFER_PAGE_COUNT];

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

/* Claims enough 256-byte arena pages for size bytes and records them in a free slot.
 *
 * Scans the page occupancy map first-fit. When no run of free pages or no free slot
 * remains, it optionally prints a diagnostic, yields the thread, and rescans.
 * The arena holds VARIANT_MENU_BUFFER_PAGE_COUNT pages (53 in BATTLE, 112 in WORLD). */
void* menu_alloc_memory(s32 size) {
    s32 page;
    s32 pages;
    s32 i;
    u8* buffer;

    size = (size & 0xFF) != 0 ? (size >> 8) + 1 : size >> 8;
    for (;;) {
        for (page = 0; page < VARIANT_MENU_BUFFER_PAGE_COUNT; page++) {
            if (g_menu_buffer_allocation_map[page] == 0) {
                for (pages = 0;; pages++) {
                    if (pages + page >= VARIANT_MENU_BUFFER_PAGE_COUNT) {
                        /* Leaves the run and page loops for the retry wait. */
                        goto fail;
                    }
                    if (pages >= size || g_menu_buffer_allocation_map[pages + page] != 0) {
                        break;
                    }
                }
                if (pages >= size) {
                    for (pages = 0; pages < size; pages++) {
                        g_menu_buffer_allocation_map[page + pages] = 1;
                    }
                    for (i = 0; i < MENU_BUFFER_ALLOCATION_COUNT; i++) {
                        if (g_menu_allocator_slot_records[i].buffer == (u8*)-1) {
                            buffer = g_menu_buffer_arena[page];
                            g_menu_allocator_slot_records[i].buffer = buffer;
                            g_menu_allocator_slot_records[i].page_count = pages;
                            return buffer;
                        }
                    }
                    break;
                }
            }
        }
    fail:
        if (g_debug_font_print_enabled != 0) {
            FntPrint(g_menu_buffer_wait_message, g_thread_current_id, size);
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
    u32 grid_base;
    s32* allocation_size;
    menu_allocation_t* allocation;
    s32* allocation_end;
    u8* allocation_grid;
    s32 invalid_buffer;
    s32 page;

    if (buffer < (u32)g_heap_companion_overlay_load_address && buffer != -1) {
        invalid_buffer = -1;
        grid_base = (buffer - (u32)g_menu_buffer_arena) >> 8;
        allocation_grid = g_menu_buffer_allocation_map;
        /* A separate page-count cursor preserves the native count-field base. */
        allocation_size = &g_menu_allocator_slot_records[0].page_count;
        allocation = g_menu_allocator_slot_records;
        allocation_end = &g_menu_allocator_slot_records[MENU_BUFFER_ALLOCATION_COUNT].page_count;
        for (;;) {
            if ((u32)allocation->buffer == buffer) {
                allocation->buffer = (u8*)invalid_buffer;
                /* Written as (index + base) + grid so the two addu operands
                 * come out in the target's order; the size is reloaded
                 * each iteration because the byte store may alias it. */
                for (page = 0; page < *allocation_size; page++) {
                    *(u8*)((page + grid_base) + (u32)allocation_grid) = 0;
                }
                return;
            }
            allocation_size += sizeof(menu_allocation_t) / sizeof(*allocation_size);
            allocation++;
            if ((s32)allocation_size >= (s32)allocation_end) {
                thread_exit_current();
                return;
            }
        }
    }
}
