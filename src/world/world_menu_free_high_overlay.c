#include "fft/main_heap.h"

void world_menu_free_high_overlay(void) {
    main_heap_free(g_main_heap_high_overlay_load_address);
}
