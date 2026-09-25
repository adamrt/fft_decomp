#include "fft/main.h"
#include "psx/types.h"

s32 main_heap_free_smd(void* ptr) {
    u32 index;
    s32 tag;
    u8* entry;

    index = ((u32)ptr - (u32)g_main_heap_smd_base) >> 11;
    tag = g_main_heap_smd_allocator_table[index];
    if (((index == 0) | (tag != g_main_heap_smd_allocator_table[index - 1])) == 0) {
        return 0;
    }
    entry = &g_main_heap_smd_allocator_table[index];
    do {
        *entry = 0;
        entry += 1;
    } while (*entry == tag);
    return 1;
}
