/* First-fit allocation of menu image buffers from the program's page arena. */

#include "fft/menu/menu.h"
#include "fft/sys/memory.h"
#include "fft/sys/system.h"
#include "fft/sys/thread.h"
#include "fft/text/text.h"
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

static menu_allocation_t s_menu_allocator_slot_records[MENU_BUFFER_ALLOCATION_COUNT];

/* WORLD's g_ai_workspace pointer cell aliases bytes inside this arena. */
static u8 s_menu_buffer_arena[VARIANT_MENU_BUFFER_PAGE_COUNT][MENU_BUFFER_PAGE_SIZE];

/* Nonzero for each arena page in use. */
static u8 s_menu_buffer_allocation_map[VARIANT_MENU_BUFFER_PAGE_COUNT];

/* Reset the menu arena allocator: every allocation slot unused (-1), every arena page free, and
 * the three typewriter glyph slots unowned. */
void menu_init_buffer_allocator(void) {
    s32 i;
    u8* page;

    for (i = MENU_BUFFER_ALLOCATION_COUNT - 1; i >= 0; i--) {
        s_menu_allocator_slot_records[i].buffer = (u8*)-1;
    }
    /* The original clears the map through a walking pointer, set after its counter. */
    i = VARIANT_MENU_BUFFER_PAGE_COUNT - 1;
    page = &s_menu_buffer_allocation_map[VARIANT_MENU_BUFFER_PAGE_COUNT - 1];
    for (; i >= 0; i--) {
        *page-- = 0;
    }
    for (i = 0; i < 3; i++) {
        g_text_slot_owner_thread_ids[i] = 0;
        g_text_slot_glyph_counts[i] = 0;
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
            if (s_menu_buffer_allocation_map[first_page] == 0) {
                for (run_pages = 0;; run_pages++) {
                    if (run_pages + first_page >= VARIANT_MENU_BUFFER_PAGE_COUNT) {
                        /* Leaves the run and page loops for the retry wait. */
                        goto fail;
                    }
                    if (run_pages >= size || s_menu_buffer_allocation_map[run_pages + first_page] != 0) {
                        break;
                    }
                }
                if (run_pages >= size) {
                    for (run_pages = 0; run_pages < size; run_pages++) {
                        s_menu_buffer_allocation_map[first_page + run_pages] = 1;
                    }
                    for (slot = 0; slot < MENU_BUFFER_ALLOCATION_COUNT; slot++) {
                        if (s_menu_allocator_slot_records[slot].buffer == (u8*)-1) {
                            buffer = s_menu_buffer_arena[first_page];
                            s_menu_allocator_slot_records[slot].buffer = buffer;
                            s_menu_allocator_slot_records[slot].page_count = run_pages;
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
    s32 first_page;
    s32 slot;
    s32 page;

    if (buffer < (u32)g_heap_companion_overlay_load_address && buffer != -1) {
        first_page = (buffer - (u32)s_menu_buffer_arena) >> 8;
        for (slot = 0; slot < MENU_BUFFER_ALLOCATION_COUNT; slot++) {
            if ((u32)s_menu_allocator_slot_records[slot].buffer == buffer) {
                s_menu_allocator_slot_records[slot].buffer = (u8*)-1;
                for (page = 0; page < s_menu_allocator_slot_records[slot].page_count; page++) {
                    s_menu_buffer_allocation_map[page + first_page] = 0;
                }
                return;
            }
        }
        thread_current_exit();
    }
}
