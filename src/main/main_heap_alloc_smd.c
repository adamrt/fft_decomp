#include "fft/main.h"
#include "psx/types.h"

void* main_heap_alloc_smd(u32 size) {
    u32 run = 0;
    u32 needed = size >> 11;
    u32 tag = 1;
    u32 index;
    u32 start;

    if ((size & 0x7ff) != 0) {
        needed++;
    }
    index = 0;
    do {
        u32 cell;

        cell = g_main_heap_smd_allocator_table[index];
        if (cell == 0) {
            if (run == 0) {
                start = index;
            }
            run++;
            if (run >= needed) {
                break;
            }
        } else {
            if (cell >= tag) {
                tag = cell + 1;
            }
            run = 0;
        }
        index++;
    } while (index < 16);
    if (run >= needed) {
        for (index = 0; index < run; index++) {
            g_main_heap_smd_allocator_table[start + index] = tag;
        }
        return &g_main_heap_smd_base[start << 11];
    }
    return 0;
}
