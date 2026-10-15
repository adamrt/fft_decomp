#include "fft/battle_gfx.h"

/* Free a slot's image buffer if it holds an unpacked EVTCHR block. */
s32 battle_gfx_clear_loaded_evtchr_slot(s32 slot_index) {
    if (g_battle_gfx_vram_slots[slot_index].image_buffer_state == BATTLE_GFX_VRAM_SLOT_BUFFER_EVTCHR) {
        g_battle_gfx_vram_slots[slot_index].image_buffer_state = BATTLE_GFX_VRAM_SLOT_BUFFER_FREE;
        g_battle_gfx_vram_slots[slot_index].evtchr_load_state = 0;
        return 1;
    }

    return 0;
}
