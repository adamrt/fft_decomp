#include "fft/battle_gfx.h"

/* SaveEVTCHR: upload the slot's unpacked EVTCHR image to its page, if the
 * page is free. */
s32 battle_gfx_save_evtchr_slot(s32 slot_index) {
    if (g_battle_gfx_vram_slots[slot_index].owner == BATTLE_GFX_VRAM_SLOT_OWNER_NONE) {
        LoadImage(
            &g_battle_gfx_vram_slots[slot_index].image_rect, (u32*)g_battle_gfx_vram_slots[slot_index].image_data);
        g_battle_gfx_vram_slots[slot_index].owner = BATTLE_GFX_VRAM_SLOT_OWNER_EVTCHR;
        return 1;
    }

    return 0;
}
