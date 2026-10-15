#include "fft/battle_gfx.h"

/*
 * Describe both battle VRAM slots: the owning unit's spritesheet (or the
 * free/saved marker) and the EVTCHR load marker, collapsed to 0xfd..0xff.
 *
 * A struct index into g_battle_gfx_spritesheet_slots keeps the table address out of the loop
 * invariants; the flat byte-array form hoists it into a saved register.
 */
s32 battle_gfx_get_vram_slot_summary(battle_gfx_vram_slot_summary_t* summary) {
    s32 i;
    u32 owner;
    u32 marker;

    for (i = 0; i < 2; i++) {
        owner = g_battle_gfx_vram_slots[i].owner;
        if (owner == BATTLE_GFX_VRAM_SLOT_OWNER_NONE) {
            summary[i].spritesheet_id = 0xff;
        } else if (owner == BATTLE_GFX_VRAM_SLOT_OWNER_EVTCHR) {
            summary[i].spritesheet_id = 0xfe;
        } else if (owner & BATTLE_GFX_VRAM_SLOT_OWNER_ATTACK_IMAGE) {
            summary[i].spritesheet_id
                = g_battle_gfx_spritesheet_slots[battle_unit_get_by_runtime_id(g_battle_gfx_vram_slots[i].owner & 0x7f)
                                                     ->spritesheet_vram_slot]
                      .spritesheet_id;
        } else {
            summary[i].spritesheet_id = 0xfd;
        }
        marker = g_battle_gfx_vram_slots[i].image_buffer_state;
        if (marker != BATTLE_GFX_VRAM_SLOT_BUFFER_EVTCHR) {
            if (marker == BATTLE_GFX_VRAM_SLOT_BUFFER_FREE) {
                summary[i].evtchr_state = 0xff;
                continue;
            }
            marker = 0xfd;
        }
        summary[i].evtchr_state = marker;
    }
    return 1;
}
