#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Converts the 16x16 item icon sprite for one item into a texture source
 * record: texture origin and CLUT from the shared icon-sprite helper, plus the
 * fixed icon texture page at VRAM (0x380, 0x120). */
void world_gfx_build_item_graphic_descriptor(menu_icon_source_t* source, s32 item_id) {
    SPRT sprite;

    gfx_set_item_icon_sprite(&sprite, item_id);
    source->u = sprite.u0;
    source->v = sprite.v0;
    source->w = 16;
    source->h = 16;
    source->clut = sprite.clut;
    source->tpage = GetTPage(0, 0, 0x380, 0x120);
}
