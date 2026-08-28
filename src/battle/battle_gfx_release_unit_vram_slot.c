#include "fft/battle_gfx.h"

void battle_gfx_release_unit_vram_slot(u8* unit) {
    s32 i;
    s32 slot_id;

    for (i = 0; i < 2; i++) {
        slot_id = g_battle_gfx_vram_slots[i].owner;
        if ((u32)(slot_id - 0xfe) >= 2 && (slot_id & 0x1f) == unit[4] && (slot_id & 0x40) == 0) {
            g_battle_gfx_vram_slots[i].owner = 0xff;
            g_battle_gfx_vram_slots[i].evtchr_load_state = 0;
        }
    }
}
