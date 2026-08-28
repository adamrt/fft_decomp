#include "fft/world.h"
#include "psx/types.h"

extern u8* g_event_overlay_load_address;

/* WORLD twin of battle_menu_free_memory. */
void world_menu_free_memory(void* buffer_pointer) {
    u32 buffer = (u32)buffer_pointer;
    u32 grid_base;
    s32* allocation_size;
    s32* allocation_buffer;
    s32* allocation_end;
    u8* allocation_grid;
    s32 invalid_buffer;
    s32 page;

    if (buffer < (u32)g_event_overlay_load_address && buffer != (u32)-1) {
        invalid_buffer = -1;
        grid_base = (buffer - (u32)g_world_menu_buffer_arena) >> 8;
        allocation_grid = g_world_menu_buffer_allocation_map;
        allocation_size = &g_world_menu_buffer_allocations[0].page_count;
        allocation_buffer = allocation_size - 1;
        allocation_end = allocation_size + 32;
        for (;;) {
            if (*allocation_buffer == buffer) {
                *allocation_buffer = invalid_buffer;
                /* Written as (index + base) + grid so the two addu operands
                 * come out in the target's order; the size is reloaded
                 * each iteration because the byte store may alias it. */
                for (page = 0; page < *allocation_size; page++) {
                    *(u8*)((page + grid_base) + (u32)allocation_grid) = 0;
                }
                return;
            }
            allocation_size += 2;
            allocation_buffer += 2;
            if ((s32)allocation_size >= (s32)allocation_end) {
                world_thread_exit_current();
                return;
            }
        }
    }
}
