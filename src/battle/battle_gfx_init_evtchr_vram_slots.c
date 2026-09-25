#include "fft/battle.h"
#include "psx/types.h"

s32 battle_gfx_init_evtchr_vram_slots(void) {
    s32 slot_index;

    for (slot_index = 0; slot_index < 2; slot_index++) {
        g_battle_gfx_vram_slots[slot_index].owner = 0xff;
        g_battle_gfx_vram_slots[slot_index].evtchr_load_marker = 0;
        g_battle_gfx_vram_slots[slot_index].evtchr_load_state = 0;
    }

    return 1;
}
