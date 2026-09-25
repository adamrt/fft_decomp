/*
 * Loads a unit spritesheet into spritesheet slot unit->spritesheet_vram_slot: the first
 * 0x200 bytes become the slot's 16 CLUTs with the STP bit (0x80 in each high
 * byte) set on every colour except index 0, two texture blocks go to VRAM
 * column `index` (256x256 at x=0x100+index*0x40, y=0x100; 256x32 in the
 * 0x340 area), and 0x30d4 bytes from +0x9200 are copied into the slot.
 *
 * Indexed loops (not pointer walks) are required: strength reduction places
 * the pointer inits after the hoisted `1` constant, as in the target. The
 * three RECT locals reproduce the 0x50-byte frame; only the middle one is
 * written (dead x/y stores the target keeps).
 */
#include "fft/battle.h"
#include "psx/types.h"

void battle_gfx_load_spritesheet_into_vram_slot(u8* data, u32 index, s32 unused, battle_unit_misc_data_t* unit) {
    u8* palette;
    u8* image;
    s32 i;
    RECT unused_rect;
    RECT rect;
    RECT unused_rect2;

    palette = g_battle_gfx_spritesheet_slots[unit->spritesheet_vram_slot].palettes[0];
    image = g_battle_gfx_spritesheet_slots[unit->spritesheet_vram_slot]._unknown_0002;
    if (data != 0) {
        for (i = 0; i < 0x200; i++) {
            if ((i & 1) && (i % 32 != 1)) {
                palette[i] = data[i] | 0x80;
            } else {
                palette[i] = data[i];
            }
        }
        data += 0x200;
        rect.x = (index << 6) + 0x100;
        rect.y = 0x100;
        LoadTPage((u32*)data, 0, 0, rect.x, 0x100, 0x100, 0x100);
        rect.x = ((index >> 3) << 6) + 0x340;
        rect.y = ((index & 7) << 5) + 0x100;
        LoadTPage((u32*)(data + 0x8000), 0, 0, rect.x, rect.y, 0x100, 0x20);
        for (i = 0; i < 0x30d4; i++) {
            image[i] = data[i + 0x9000];
        }
    }
}
