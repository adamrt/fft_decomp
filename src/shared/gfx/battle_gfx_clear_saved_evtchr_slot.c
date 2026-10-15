#include "fft/battle_gfx.h"

/* Free a slot's page if it holds an EVTCHR image. */
s32 battle_gfx_clear_saved_evtchr_slot(s32 slot_index) {
    if (g_battle_gfx_vram_slots[slot_index].owner == BATTLE_GFX_VRAM_SLOT_OWNER_EVTCHR) {
        g_battle_gfx_vram_slots[slot_index].owner = BATTLE_GFX_VRAM_SLOT_OWNER_NONE;
        return 1;
    }

    return 0;
}
