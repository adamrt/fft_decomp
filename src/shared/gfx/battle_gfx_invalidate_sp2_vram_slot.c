#include "fft/battle_gfx.h"

/* Free the EVTCHR slots whose page holds the unit's SP2 image. */
void battle_gfx_invalidate_sp2_vram_slot(battle_unit_t* unit) {
    u32 invalid;
    battle_gfx_vram_slot_t* slot;
    s32 count;

    count = 0;
    invalid = BATTLE_GFX_VRAM_SLOT_OWNER_NONE;
    slot = g_battle_gfx_vram_slots;
    for (; count < 2; count++, slot++) {
        u32 descriptor = slot->owner;

        if (((descriptor & BATTLE_GFX_VRAM_SLOT_OWNER_ID_MASK) == unit->runtime_id)
            && ((descriptor & BATTLE_GFX_VRAM_SLOT_OWNER_SP2) != 0)) {
            slot->owner = invalid;
        }
    }
}
