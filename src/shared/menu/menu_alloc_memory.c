#include "fft/battle.h"
#include "fft/battle_menu.h"
#include "fft/battle_text.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Claims enough 256-byte arena pages for size bytes and records them in a free slot.
 *
 * Scans the page occupancy map first-fit. When no run of free pages or no free slot
 * remains, it optionally prints a diagnostic, yields the thread, and rescans.
 *
 * BATTLE twin of menu_alloc_memory. */
void* menu_alloc_memory(s32 size) {
    s32 page;
    s32 n;
    s32 i;
    u8* buffer;

    size = (size & 0xFF) != 0 ? (size >> 8) + 1 : size >> 8;
    for (;;) {
        for (page = 0; page < BATTLE_MENU_BUFFER_PAGE_COUNT; page++) {
            if (g_battle_menu_buffer_allocation_map[page] == 0) {
                for (n = 0;; n++) {
                    if (n + page >= BATTLE_MENU_BUFFER_PAGE_COUNT) {
                        /* Leaves both scans for the wait-and-retry path. */
                        goto fail;
                    }
                    if (n >= size || g_battle_menu_buffer_allocation_map[n + page] != 0) {
                        break;
                    }
                }
                if (n >= size) {
                    for (n = 0; n < size; n++) {
                        g_battle_menu_buffer_allocation_map[page + n] = 1;
                    }
                    for (i = 0; i < MENU_BUFFER_ALLOCATION_COUNT; i++) {
                        if (g_battle_menu_allocator_slot_records[i].buffer == (u8*)-1) {
                            buffer = g_battle_menu_buffer_arena[page];
                            g_battle_menu_allocator_slot_records[i].buffer = buffer;
                            g_battle_menu_allocator_slot_records[i].page_count = n;
                            return buffer;
                        }
                    }
                    break;
                }
            }
        }
    fail:
        if (g_main_font_print_enabled != 0) {
            FntPrint(g_battle_text_wait_for_allocation_message, g_thread_current_id, size);
        }
        thread_yield();
    }
}
