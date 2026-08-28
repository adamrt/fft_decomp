#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "psx/types.h"

s32 battle_gfx_load_misc_unit_into_evtchr_slot(s32 misc_id, s32 slot) {
    battle_unit_misc_data_t* unit;
    s32 image_attempt;
    s32 current_image_offset;
    /* Pin: keeps the slot offset in $s2, apart from its per-iteration copy in $v1. */
    register s32 image_offset __asm__("$18");
    /* Pin: the three state stores go through $v1 (unpinned: $a0). */
    register s32 state __asm__("$3");
    u8* image_data;
    RECT rect;

    unit = battle_unit_get_misc_data_by_misc_id((u16)misc_id);
    if (g_battle_gfx_vram_slots[slot].owner != 0xfe) {
        return 0;
    }

    image_attempt = 0;
    image_offset = 0;
    do {
        image_attempt++;
        current_image_offset = image_offset;
        if (((battle_gfx_vram_slot_t*)((u8*)g_battle_gfx_vram_slots + current_image_offset))->evtchr_load_marker == 0) {
            image_data = ((battle_gfx_vram_slot_t*)((u8*)g_battle_gfx_vram_slots + image_offset))->image_data;
            battle_gfx_decompress_attack_spritesheet(
                g_battle_gfx_spritesheet_slots[unit->spritesheet_vram_slot]._unknown02, image_data);
            LoadImage(&g_battle_gfx_vram_slots[slot].image_rect, (u32*)image_data);
            rect.x = (unit->spritesheet_vram_slot >> 3) * 64 + 0x340;
            rect.y = (unit->spritesheet_vram_slot & 7) * 32 + 0x100;
            rect.w = 0x40;
            rect.h = 0x20;
            MoveImage(
                &rect, g_battle_gfx_vram_slots[slot].image_rect.x, g_battle_gfx_vram_slots[slot].image_rect.y + 200);
            state = misc_id + 0x80;
            g_battle_gfx_vram_slots[slot].owner = state;
            state = 2;
            g_battle_gfx_vram_slots[slot].evtchr_load_marker = state;
            state = 1;
            g_battle_gfx_vram_slots[slot].evtchr_load_state = state;
            /* Keeps the success return's `li v0,1` after the final store. */
            __asm__ volatile("");
            return 1;
        }
        image_offset = current_image_offset + sizeof(battle_gfx_vram_slot_t);
    } while (image_attempt < 2);
    return 0;
}
