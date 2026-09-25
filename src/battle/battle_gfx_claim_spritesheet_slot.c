#include "fft/battle.h"
#include "psx/types.h"

/* Claim a VRAM spritesheet slot for `spritesheet_id`: reuse the slot that
 * already holds it, otherwise take the first free one, and raise animation
 * exception 0xB when all nine are taken. */
s32 battle_gfx_claim_spritesheet_slot(s16 spritesheet_id) {
    s32 slot;
    s32 found;

    found = 0xFF;
    for (slot = 0; slot < 9; slot++) {
        if (g_battle_gfx_spritesheet_slots[slot].spritesheet_id == spritesheet_id) {
            g_battle_gfx_spritesheet_slots[slot].in_use = spritesheet_id;
            found = slot;
            break;
        }
    }
    if ((found & 0xFFFF) == 0xFF) {
        for (slot = 0; slot < 9; slot++) {
            if (g_battle_gfx_spritesheet_slots[slot].in_use == 0) {
                g_battle_gfx_spritesheet_slots[slot].in_use = spritesheet_id;
                g_battle_gfx_spritesheet_slots[slot].spritesheet_id = spritesheet_id;
                found = slot;
                break;
            }
        }
    }
    if (slot >= 9) {
        animation_exception_handler(0xB);
        return 0xFFFF;
    }
    return found & 0xFFFF;
}
