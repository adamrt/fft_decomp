#include "fft/main_runtime.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Claims enough 256-byte arena pages for size bytes and records them in a free slot.
 *
 * Scans the page occupancy map first-fit. When no run of free pages or no free slot
 * remains, it optionally prints a diagnostic, yields the thread, and rescans. */
void* world_menu_alloc_ui_buffer(s32 size) {
    s32 page;
    s32 pages;
    s32 i;
    u8* buffer;

    size = (size & 0xFF) != 0 ? (size >> 8) + 1 : size >> 8;
    for (;;) {
        for (page = 0; page < 0x70; page++) {
            if (g_world_menu_buffer_allocation_map[page] == 0) {
                for (pages = 0;; pages++) {
                    if (pages + page >= 0x70) {
                        /* Leaves the run and page loops for the retry wait. */
                        goto fail;
                    }
                    if (pages >= size || g_world_menu_buffer_allocation_map[pages + page] != 0) {
                        break;
                    }
                }
                if (pages >= size) {
                    for (pages = 0; pages < size; pages++) {
                        g_world_menu_buffer_allocation_map[page + pages] = 1;
                    }
                    for (i = 0; i < 16; i++) {
                        if (g_world_menu_buffer_allocations[i].buffer == (u8*)-1) {
                            buffer = g_world_menu_buffer_arena[page];
                            g_world_menu_buffer_allocations[i].buffer = buffer;
                            g_world_menu_buffer_allocations[i].page_count = pages;
                            return buffer;
                        }
                    }
                    break;
                }
            }
        }
    fail:
        if (g_font_print_enabled != 0) {
            FntPrint(g_world_text_wait_for_allocation_message, g_world_thread_current_id, size);
        }
        world_thread_yield();
    }
}
