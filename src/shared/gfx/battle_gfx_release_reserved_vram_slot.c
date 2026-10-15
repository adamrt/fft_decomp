#include "fft/battle_gfx.h"
#include "psx/types.h"

/* Free a slot's page if it holds a unit's attack image. */
s32 battle_gfx_release_reserved_vram_slot(s32 slot_index) {
    if ((g_battle_gfx_vram_slots[slot_index].owner & BATTLE_GFX_VRAM_SLOT_OWNER_ATTACK_IMAGE) != 0) {
        g_battle_gfx_vram_slots[slot_index].owner = BATTLE_GFX_VRAM_SLOT_OWNER_NONE;
        g_battle_gfx_vram_slots[slot_index].evtchr_load_state = 0;
        return 1;
    }

    return 0;
}
