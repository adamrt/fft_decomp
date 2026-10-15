#include "fft/main.h"
#include "fft/menu.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "psx/libgpu.h"
#include "psx/types.h"

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
        if (g_main_font_print_enabled != 0) {
            FntPrint(g_text_wait_for_allocation_message, g_thread_current_id, size);
        }
        thread_yield();
    }
}
