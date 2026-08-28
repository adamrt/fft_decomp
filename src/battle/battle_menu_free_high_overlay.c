#include "fft/main_heap.h"
#include "psx/types.h"

void battle_menu_free_high_overlay(void) {
    main_heap_free(g_main_heap_high_overlay_load_address);
}
