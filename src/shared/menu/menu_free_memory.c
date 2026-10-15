#include "fft/battle_menu.h"
#include "fft/main.h"
#include "fft/menu.h"
#include "fft/thread.h"

/* Free a menu arena allocation; an unknown eligible pointer stops the current thread. */
void menu_free_memory(void* buffer_pointer) {
    u32 buffer = (u32)buffer_pointer;
    u32 grid_base;
    s32* allocation_size;
    menu_allocation_t* allocation;
    s32* allocation_end;
    u8* allocation_grid;
    s32 invalid_buffer;
    s32 page;

    if (buffer < (u32)g_main_heap_companion_overlay_load_address && buffer != -1) {
        invalid_buffer = -1;
        grid_base = (buffer - (u32)g_menu_buffer_arena) >> 8;
        allocation_grid = g_menu_buffer_allocation_map;
        /* A separate page-count cursor preserves the native count-field base. */
        allocation_size = &g_menu_allocator_slot_records[0].page_count;
        allocation = g_menu_allocator_slot_records;
        allocation_end = &g_menu_allocator_slot_records[MENU_BUFFER_ALLOCATION_COUNT].page_count;
        for (;;) {
            if ((u32)allocation->buffer == buffer) {
                allocation->buffer = (u8*)invalid_buffer;
                /* Written as (index + base) + grid so the two addu operands
                 * come out in the target's order; the size is reloaded
                 * each iteration because the byte store may alias it. */
                for (page = 0; page < *allocation_size; page++) {
                    *(u8*)((page + grid_base) + (u32)allocation_grid) = 0;
                }
                return;
            }
            allocation_size += sizeof(menu_allocation_t) / sizeof(*allocation_size);
            allocation++;
            if ((s32)allocation_size >= (s32)allocation_end) {
                thread_exit_current();
                return;
            }
        }
    }
}
