/* The texture source of a 16x16 item icon. */

#include "fft/game/item.h"
#include "fft/gfx/gfx_image.h"
#include "fft/menu/menu.h"
#include "fft/party/item_list.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Build the 16x16 item icon texture source. */
void item_list_build_icon_source(menu_icon_source_t* output, u32 item_id) {
    SPRT graphic;

    item_set_icon_sprite(&graphic, item_id);
    output->u = graphic.u0;
    output->v = graphic.v0;
    output->w = 16;
    output->h = 16;
    output->clut = graphic.clut;
    output->tpage = GetTPage(0, 0, 0x380, 0x120);
}
