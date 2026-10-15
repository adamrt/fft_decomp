#include "fft/battle_gfx.h"
#include "psx/types.h"

/* Free both EVTCHR slots, pages and image buffers. */
s32 battle_gfx_init_evtchr_vram_slots(void) {
    s32 slot_index;

    for (slot_index = 0; slot_index < 2; slot_index++) {
        g_battle_gfx_vram_slots[slot_index].owner = BATTLE_GFX_VRAM_SLOT_OWNER_NONE;
        g_battle_gfx_vram_slots[slot_index].image_buffer_state = BATTLE_GFX_VRAM_SLOT_BUFFER_FREE;
        g_battle_gfx_vram_slots[slot_index].evtchr_load_state = 0;
    }

    return 1;
}
