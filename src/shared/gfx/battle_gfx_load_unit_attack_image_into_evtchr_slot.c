#include "fft/battle_gfx.h"
#include "psx/types.h"

/* Put a unit's attack image on the page of a slot that holds an EVTCHR image,
 * for the event instruction that asks for it: decompress the image into a
 * free buffer, upload it and give the slot to the unit. Returns 0 when the
 * slot is in another state or no buffer is free. The buffer marked as
 * uploading is the slot's own, whichever one was used. */
s32 battle_gfx_load_unit_attack_image_into_evtchr_slot(s32 runtime_id, s32 slot) {
    battle_unit_t* unit;
    s32 i;
    u8* image_data;
    RECT rect;

    unit = battle_unit_get_by_runtime_id(runtime_id);
    if (g_battle_gfx_vram_slots[slot].owner == BATTLE_GFX_VRAM_SLOT_OWNER_EVTCHR) {
        for (i = 0; i < 2; i++) {
            if (g_battle_gfx_vram_slots[i].image_buffer_state == BATTLE_GFX_VRAM_SLOT_BUFFER_FREE) {
                image_data = g_battle_gfx_vram_slots[i].image_data;
                battle_gfx_decompress_attack_spritesheet(
                    g_battle_gfx_spritesheet_slots[unit->spritesheet_vram_slot].compressed_attack_image, image_data);
                LoadImage(&g_battle_gfx_vram_slots[slot].image_rect, (u32*)image_data);
                rect.x = (unit->spritesheet_vram_slot >> 3) * 64 + 0x340;
                rect.y = (unit->spritesheet_vram_slot & 7) * 32 + 0x100;
                rect.w = 0x40;
                rect.h = 0x20;
                MoveImage(&rect, g_battle_gfx_vram_slots[slot].image_rect.x,
                    g_battle_gfx_vram_slots[slot].image_rect.y + 200);
                g_battle_gfx_vram_slots[slot].owner = runtime_id + BATTLE_GFX_VRAM_SLOT_OWNER_ATTACK_IMAGE;
                g_battle_gfx_vram_slots[slot].image_buffer_state = BATTLE_GFX_VRAM_SLOT_UPLOAD_FRAMES;
                g_battle_gfx_vram_slots[slot].evtchr_load_state = 1;
                return 1;
            }
        }
        /* Retail has this exit and the one below as two separate returns. */
        return 0;
    }
    return 0;
}
