#include "fft/world.h"
#include "psx/types.h"

void world_menu_reset_state_arrays(void) {
    s32 fill;
    s32 slot;
    s32 count;
    u8* byte;
    s32* first;
    s32* second;

    /* One counter local shared by the last two loops and each counter set
     * before its pointer: this reproduces the target's register assignment
     * without pins. */
    fill = -1;
    for (slot = 15; slot >= 0; slot--) {
        g_world_menu_buffer_allocations[slot].buffer = (u8*)fill;
    }
    count = 0x6f;
    byte = &g_world_menu_buffer_allocation_map[0x6f];
    for (; count >= 0; count--) {
        *byte = 0;
        byte--;
    }
    count = 0;
    second = g_world_menu_slot_states;
    first = g_world_menu_slot_owner_thread_ids;
    for (; count < 3; count++) {
        *first = 0;
        *second = 0;
        second++;
        first++;
    }
}
