#include "fft/battle_gfx.h"
#include "psx/types.h"

/* Upload both uncompressed images, copy the compressed attack image,
 * and enable STP on each palette's entries 1..15.
 * Indexed byte copies preserve the original loop scheduling. Three RECT
 * locals preserve the 0x50-byte frame and its otherwise unused x/y stores. */
void battle_gfx_load_spritesheet_into_vram_slot(
    const battle_gfx_unit_sprite_file_t* source, u32 index, s32 unused, battle_unit_t* unit, s16 unused_palette) {
    const u8* data;
    u32 column;
    u8* clut_bytes;
    u8* compressed_attack_image;
    s32 byte_index;
    RECT unused_rect;
    RECT rect;
    RECT unused_rect2;

    /* Byte stores preserve the palette loop and shared image cursor. */
    data = (const u8*)source;
    column = index;
    clut_bytes = (u8*)g_battle_gfx_spritesheet_slots[unit->spritesheet_vram_slot].clut[0];
    compressed_attack_image = g_battle_gfx_spritesheet_slots[unit->spritesheet_vram_slot].compressed_attack_image;
    if (data != 0) {
        for (byte_index = 0; byte_index < 0x200; byte_index++) {
            if (byte_index % 2 != 0 && byte_index % 32 != 1) {
                clut_bytes[byte_index] = data[byte_index] | 0x80;
            } else {
                clut_bytes[byte_index] = data[byte_index];
            }
        }
        data += sizeof(source->clut);
        rect.x = column * 64 + 0x100;
        rect.y = 0x100;
        LoadTPage((u32*)data, 0, 0, rect.x, 0x100, 0x100, 0x100);
        rect.x = (column / 8) * 64 + 0x340;
        rect.y = (column % 8) * 32 + 0x100;
        LoadTPage((u32*)(data + sizeof(source->pixels)), 0, 0, rect.x, rect.y, 0x100, 0x20);
        for (byte_index = 0; byte_index < 0x30d4; byte_index++) {
            compressed_attack_image[byte_index]
                = data[byte_index + sizeof(source->pixels) + sizeof(source->portrait_and_body_pixels)];
        }
    }
}
