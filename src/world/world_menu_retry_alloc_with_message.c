#include "fft/world.h"
#include "psx/types.h"

/* WORLD twin of battle_menu_retry_alloc_with_message: retry the overlay
 * allocation until it lands at the expected address, yielding and printing
 * a debug message each pass. */
void world_menu_retry_alloc_with_message(u32 size) {
    while (1) {
        if (main_heap_reserve_at(g_main_heap_high_overlay_load_address, size)
            == g_main_heap_high_overlay_load_address) {
            break;
        }
        world_thread_yield();
        if (g_font_print_enabled != 0) {
            FntPrint(g_world_text_wait_for_file_allocation_message);
        }
    }
}
