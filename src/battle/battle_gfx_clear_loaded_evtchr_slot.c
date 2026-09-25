#include "fft/battle.h"

s32 battle_gfx_clear_loaded_evtchr_slot(s32 slot_index) {
    if (g_battle_gfx_vram_slots[slot_index].evtchr_load_marker == 0xfe) {
        g_battle_gfx_vram_slots[slot_index].evtchr_load_marker = 0;
        g_battle_gfx_vram_slots[slot_index].evtchr_load_state = 0;
        return 1;
    }

    return 0;
}
