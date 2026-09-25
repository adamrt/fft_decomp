#include "fft/battle.h"

void battle_effect_init_palette(s16 index) {
    RECT rect;
    effect_file_header_t* base;
    u8* data;
    s32 size;
    s32 raw_size;
    s32 height;

    base = g_battle_effect_data_ptrs[index];
    data = (u8*)base + base->palette;
    g_battle_effect_palette_ptr = data;
    battle_map_load_palette_data((const u16*)data, 0xC, 0, 1);
    battle_map_load_palette_data((const u16*)(data + EFFECT_FILE_PALETTE_SECOND_BLOCK_OFFSET), 0xD, 0, 1);
    raw_size = data[EFFECT_FILE_PALETTE_SIZE_OFFSET] + (data[EFFECT_FILE_PALETTE_SIZE_OFFSET + 1] << 8);
    raw_size = (data[EFFECT_FILE_PALETTE_SIZE_OFFSET + 2] << 16) + raw_size;
    height = raw_size;
    size = height;
    if (data[EFFECT_FILE_PALETTE_SIZE_OFFSET + 3] != 0) {
        height = size / 0x100;
        rect.x = 0x180;
        rect.y = 0;
        rect.w = 0x80;
        rect.h = height;
        LoadImage(&rect, (u32*)(data + EFFECT_FILE_PALETTE_IMAGE_OFFSET));
    } else {
        height = size;
        height = height / 0x80;
        rect.x = 0x180;
        rect.y = 0;
        rect.w = 0x40;
        rect.h = height;
        LoadImage(&rect, (u32*)(data + EFFECT_FILE_PALETTE_IMAGE_OFFSET));
    }
}
