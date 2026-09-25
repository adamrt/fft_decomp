#include "fft/main.h"
#include "psx/types.h"

void main_heap_clear_game_allocator_table(void) {
    int index = 63;
    u8* entry = &g_main_heap_game_allocator_table[63];

    for (; index >= 0; index--, entry--) {
        *entry = 0;
    }
}
