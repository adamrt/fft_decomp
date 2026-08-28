#include "fft/battle.h"
#include "psx/types.h"

/*
 * Load a battle texture archive: two 0x200-byte tables and three 4bpp texture
 * pages into VRAM, with the palette block at +0x10400 sent to map mesh 0xb.
 */
void battle_gfx_load_wep_spr_data(u8* data) {
    s32 i;
    u8* cursor;

    cursor = data;
    for (i = 0; i < 0x200; i++) {
        g_battle_gfx_item_palettes[i] = *cursor++;
    }
    LoadTPage((u32*)(data + 0x200), 0, 0, 0x240, 0, 0x100, 0x100);
    for (i = 0, cursor = data + 0x8200; i < 0x200; i++) {
        g_battle_gfx_eff_palettes[i] = *cursor++;
    }
    LoadTPage((u32*)(data + 0x8400), 0, 0, 0x200, 0, 0x100, 0x100);
    battle_map_load_palette_data((u16*)(data + 0x10400), 0xb, 0, 1);
    LoadTPage((u32*)(data + 0x10600), 0, 0, 0x200, 0x70, 0x100, 0x90);
}
