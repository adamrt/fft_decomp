#include "fft/battle.h"
#include "fft/main_heap.h"
#include "fft/main_runtime.h"
#include "psx/types.h"

/* Retry the overlay allocation until it lands at the expected address, yielding and printing a debug message each pass.
 */
void battle_menu_retry_alloc_with_message(u32 size) {
    while (1) {
        if (game_realloc(g_main_heap_high_overlay_load_address, size) == g_main_heap_high_overlay_load_address) {
            break;
        }
        battle_thread_yield();
        if (g_font_print_enabled != 0) {
            FntPrint(g_battle_text_wait_for_file_allocation_message);
        }
    }
}
