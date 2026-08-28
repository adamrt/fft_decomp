#include "fft/main_heap.h"
#include "fft/main_runtime.h"
#include "psx/types.h"

s32 main_heap_free(void* pointer) {
    u32 page_index = (u32)((u8*)pointer - g_main_heap_high_overlay_load_address) >> 11;
    s32 allocation_id = g_main_heap_game_allocator_table[page_index];
    s32 previous_id = g_main_heap_game_allocator_table[page_index - 1];

    if ((page_index < 1) | (allocation_id != previous_id)) {
        u8* table = g_main_heap_game_allocator_table;
        u8* entry = &table[page_index];

        do {
            *entry++ = 0;
        } while (*entry == allocation_id);
        return 1;
    }
    return 0;
}
