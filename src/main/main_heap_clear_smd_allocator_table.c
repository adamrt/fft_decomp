#include "fft/main_runtime.h"
#include "psx/types.h"

void main_heap_clear_smd_allocator_table(void) {
    int index = 15;
    u8* entry = &g_main_heap_smd_allocator_table[15];

    for (; index >= 0; index--, entry--) {
        *entry = 0;
    }
}
