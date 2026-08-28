#include "fft/main_heap.h"
#include "fft/main_runtime.h"
#include "psx/types.h"

void* main_heap_alloc(u32 size) {
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

        cell = g_main_heap_game_allocator_table[index];
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
    } while (index < 0x40);
    if (run < needed) {
        return 0;
    }
    for (index = 0; index < run; index++) {
        /* Stops reorg copying the address add into the bnez delay slot (a nop in the target). */
        __asm__ volatile("");
        /* Integer sum first: `g_main_heap_game_allocator_table[start + index]` emits
         * `addu rd,table,sum`; the target adds the indices before the base. */
        *(u8*)((start + index) + (u32)g_main_heap_game_allocator_table) = tag;
    }
    return (start << 11) + g_main_heap_high_overlay_load_address;
}
