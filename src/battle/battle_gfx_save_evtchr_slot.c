#include "fft/battle.h"

s32 battle_gfx_save_evtchr_slot(s32 slot_index) {
    if (g_battle_gfx_vram_slots[slot_index].owner == 0xff) {
        LoadImage(
            &g_battle_gfx_vram_slots[slot_index].image_rect, (u32*)g_battle_gfx_vram_slots[slot_index].image_data);
        g_battle_gfx_vram_slots[slot_index].owner = 0xfe;
        return 1;
    }

    return 0;
}
