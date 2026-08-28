#include "fft/battle.h"
#include "fft/menu.h"
#include "psx/types.h"

void battle_menu_free_buffer(s32 buffer) {
    s32 slot;
    s32 offset;
    s32 empty;

    if (buffer == -1) {
        return;
    }

    slot = 0;
    empty = -1;
    offset = 0x118;
    do {
        if ((s32)g_battle_menu_buffer_slots[slot] == buffer) {
            g_battle_menu_buffer_slots[slot] = (void*)empty;
            g_battle_menu_buffer_slots[slot + 1] = (void*)empty;
            /* Clears active and thread_id of both buffers of the pair; offset
             * is the byte offset of the pair's second buffer, and these
             * field-address + offset forms keep the target's addressing. */
            *(s32*)((u8*)&g_battle_menu_window_buffers[-1].active + offset) = 0;
            *(s32*)((u8*)&g_battle_menu_window_buffers[0].active + offset) = 0;
            *(s32*)((u8*)&g_battle_menu_window_buffers[-1].thread_id + offset) = 0;
            *(s32*)((u8*)&g_battle_menu_window_buffers[0].thread_id + offset) = 0;
            return;
        }
        offset += 0x230;
        slot += 2;
    } while (slot < 6);

    battle_thread_exit_current();
}
