#include "fft/gfx.h"
#include "fft/main.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Fill the texture coordinates, CLUT and size of a 16x16 item icon sprite. */
void gfx_set_item_icon_sprite(SPRT* sprite, s32 item_id) {
    item_data_t* item;
    s32 sprite_id;
    s32 palette;
    s32 column;

    item = main_item_get_data_pointer(item_id);
    sprite_id = item->sprite_id;
    palette = item->palette;
    column = sprite_id % 15;
    sprite->u0 = column << 4;
    sprite->v0 = ((sprite_id / 15) << 4) + 0x20;
    sprite->clut
        = GetClut(g_gfx_item_icon_clut_base_x + ((palette % 8) << 4), g_gfx_item_icon_clut_base_y + palette / 8);
    sprite->w = 16;
    sprite->h = 16;
}
