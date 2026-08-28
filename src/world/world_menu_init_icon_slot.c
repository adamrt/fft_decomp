#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

void world_menu_init_icon_slot(RECT* rect, s32 width, s32 height, world_texture_prim_t* slot, s32 icon_index) {
    rect->w = width;
    rect->h = height;
    world_gfx_alloc_texture_grid_rect(rect, slot, (u32*)-1);
    slot->sprt.clut = (icon_index * 0x10) + 0x7C3C;
}
