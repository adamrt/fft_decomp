#include "fft/data.h"
#include "fft/main_unit.h"
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Fills the texture coordinates, CLUT and size of a 16x16 item icon sprite. */
void world_build_item_icon_sprite(SPRT* sprite, s32 item_id) {
    item_data_t* item;
    s32 sprite_id;
    s32 palette;
    s32 column;
    s32 clut_x;

    item = main_item_get_data_pointer(item_id);
    sprite_id = item->sprite_id;
    palette = item->palette;
    column = sprite_id % 15;
    sprite->u0 = column << 4;
    sprite->v0 = ((sprite_id / 15) << 4) + 0x20;
    clut_x = g_world_item_icon_clut_base_x;
    column = palette % 8;
    sprite->clut = GetClut(clut_x + (column << 4), g_world_item_icon_clut_base_y + palette / 8);
    sprite->w = 16;
    sprite->h = 16;
}
