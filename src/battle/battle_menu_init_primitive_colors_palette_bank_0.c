#include "fft/battle.h"
#include "psx/types.h"

void battle_menu_init_primitive_colors_palette_bank_0(world_menu_palette_primitives_t* menu) {
    s32 index;
    s32 palette_offset;
    LINE_F2* line;

    index = 0;
    line = menu->lines;
    do {
        palette_offset = g_battle_menu_numeric_display_palette_indices[index] * 2;
        line->r0 = (*(volatile u8*)(g_battle_menu_palette_colors + palette_offset) & 0x1f) << 3;
        line->g0 = (*(volatile u16*)(g_battle_menu_palette_colors + palette_offset) >> 2) & 0xf8;
        index++;
        line->b0 = (*(volatile u16*)(g_battle_menu_palette_colors + palette_offset) >> 7) & 0xf8;
        line++;
    } while (index < 12);
    menu->sprite.clut = 0x7c3c;
}
