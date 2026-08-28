#include "psx/gpu.h"
#include "psx/types.h"

union battle_texture_prim;
extern void battle_gfx_alloc_tpage7_vram(RECT* rect, union battle_texture_prim* sprite, u32* image_data);

void battle_text_configure_sprite_vram(RECT* rect, s16 width, s16 height, SPRT* sprite, s32 palette_row) {
    rect->w = width;
    rect->h = height;
    battle_gfx_alloc_tpage7_vram(rect, (union battle_texture_prim*)sprite, (u32*)-1);
    sprite->clut = (palette_row << 4) + 0x7c3c;
}
