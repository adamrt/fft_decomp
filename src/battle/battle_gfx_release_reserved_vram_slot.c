#include "fft/battle_gfx.h"
#include "psx/types.h"

s32 battle_gfx_release_reserved_vram_slot(s32 slot_index) {
    if ((g_battle_gfx_vram_slots[slot_index].owner & 0x80) != 0) {
        g_battle_gfx_vram_slots[slot_index].owner = 0xff;
        g_battle_gfx_vram_slots[slot_index].evtchr_load_state = 0;
        return 1;
    }

    return 0;
}
