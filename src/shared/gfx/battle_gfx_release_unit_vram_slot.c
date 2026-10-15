#include "fft/battle_gfx.h"

/* Free the EVTCHR slots whose page holds the unit's attack image. One that
 * holds its SP2 image stays. */
void battle_gfx_release_unit_vram_slot(battle_unit_t* unit) {
    s32 i;
    s32 slot_id;

    for (i = 0; i < 2; i++) {
        slot_id = g_battle_gfx_vram_slots[i].owner;
        if (slot_id != BATTLE_GFX_VRAM_SLOT_OWNER_EVTCHR && slot_id != BATTLE_GFX_VRAM_SLOT_OWNER_NONE
            && (slot_id & BATTLE_GFX_VRAM_SLOT_OWNER_ID_MASK) == unit->runtime_id
            && (slot_id & BATTLE_GFX_VRAM_SLOT_OWNER_SP2) == 0) {
            g_battle_gfx_vram_slots[i].owner = BATTLE_GFX_VRAM_SLOT_OWNER_NONE;
            g_battle_gfx_vram_slots[i].evtchr_load_state = 0;
        }
    }
}
