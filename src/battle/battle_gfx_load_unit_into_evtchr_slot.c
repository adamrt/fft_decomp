#include "fft/battle.h"
#include "psx/types.h"

/* Capture a unit's graphics into an EVTCHR VRAM cache slot.
 *
 * The unit's compressed spritesheet slot is expanded into `image_slot`'s
 * image buffer and uploaded to `slot`'s rectangle; the unit's 64x32 VRAM
 * block (column slot >> 3, row slot & 7 of the 0x340,0x100 spritesheet area)
 * is copied 200 lines below it. `slot` is then owned by the unit (id | 0x80). */
void battle_gfx_load_unit_into_evtchr_slot(battle_unit_misc_data_t* unit, s32 slot, s32 image_slot) {
    RECT rect;

    battle_gfx_decompress_attack_spritesheet(g_battle_gfx_spritesheet_slots[unit->spritesheet_vram_slot]._unknown02,
        g_battle_gfx_vram_slots[image_slot].image_data);
    LoadImage(&g_battle_gfx_vram_slots[slot].image_rect, (u32*)g_battle_gfx_vram_slots[image_slot].image_data);
    rect.x = (unit->spritesheet_vram_slot >> 3) * 64 + 0x340;
    rect.y = (unit->spritesheet_vram_slot & 7) * 32 + 0x100;
    rect.w = 0x40;
    rect.h = 0x20;
    MoveImage(&rect, g_battle_gfx_vram_slots[slot].image_rect.x, g_battle_gfx_vram_slots[slot].image_rect.y + 200);
    g_battle_gfx_vram_slots[slot].owner = unit->unit_id | 0x80;
    g_frame_pacing_suppressed = 1;
    g_battle_gfx_vram_slots[image_slot].evtchr_load_marker = 2;
    g_battle_gfx_vram_slots[slot].evtchr_load_state = 1;
}
