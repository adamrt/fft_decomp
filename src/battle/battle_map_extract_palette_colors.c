#include "fft/battle.h"
#include "psx/types.h"

enum {
    MAP_PALETTE_UPLOAD_BANK_BYTES = 0x200,
    MAP_PALETTE_ROW_COLOR_COUNT = 16,
};

void battle_map_extract_palette_colors(const u16* palette, s32 mesh_index, s32 palette_row_index) {
    s32 color_index;
    s32 selected_row;
    s32 i;
    const u16* palette_ptr;
    u16* upload_palette_base;
    u16* upload_palette_bank;
    u16* packed_color_bank;
    map_palette_color_components_t* component_bank;
    u16 color;
    map_palette_color_components_t* components;

    selected_row = palette_row_index;
    i = 0;
    component_bank = g_battle_map_palette_state.banks[mesh_index].components;
    upload_palette_base = (u16*)g_battle_map_palette_upload_words;
    upload_palette_bank = upload_palette_base + mesh_index * (MAP_PALETTE_UPLOAD_BANK_BYTES / sizeof(u16));
    packed_color_bank = g_battle_map_palette_state.banks[mesh_index].packed_colors;
    palette_ptr = palette;
    do {
        /* PlayStation CLUT words use ABBBBBGGGGGRRRRR. */
        color = *palette_ptr++;
        color_index = selected_row << 4;
        color_index += i;
        i++;
        upload_palette_bank[color_index] = color;
        components = &component_bank[color_index];
        packed_color_bank[color_index] = color;
        components->red_5bit = color & 0x1f;
        components->green_5bit = (color & 0x3e0) >> 5;
        components->blue_5bit = (color & 0x7c00) >> 10;
        components->alpha_bit = color >> 15;
    } while (i < MAP_PALETTE_ROW_COLOR_COUNT);
}
