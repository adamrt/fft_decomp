#include "fft/battle_gfx.h"

s32 battle_gfx_clear_saved_evtchr_slot(s32 slot_index) {
    if (g_battle_gfx_vram_slots[slot_index].owner == 0xfe) {
        g_battle_gfx_vram_slots[slot_index].owner = 0xff;
        return 1;
    }

    return 0;
}
