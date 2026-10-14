#include "fft/gfx.h"
#include "fft/menu.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Allocate text image VRAM for the sprite or textured quad and give it the normal menu CLUT, GetClut(0x3c0, 0x1f0),
 * plus clut_offset sixteens of the 16-bit CLUT value (4 is one VRAM line down). Callers pass 0 or 2; 2 carries out
 * of the x field: 16-bit CLUT value 0x7c5c is GetClut(0x1c0, 0x1f1). */
void text_configure_sprite_vram(RECT* rect, s32 width, s32 height, void* packet, s32 clut_offset) {
    texture_primitive_t* primitive = packet;

    rect->w = width;
    rect->h = height;
    gfx_alloc_texture_rect(rect, primitive, (u32*)-1);
    primitive->sprt.clut = (clut_offset << 4) + 0x7c3c;
}
