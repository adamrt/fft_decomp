#include "fft/battle_gfx.h"
#include "fft/menu.h"

/* Build the 16x16 item icon texture source. */
void menu_build_item_icon_source(menu_icon_source_t* output, u32 item_id) {
    SPRT graphic;

    gfx_set_item_icon_sprite(&graphic, item_id);
    output->u = graphic.u0;
    output->v = graphic.v0;
    output->w = 16;
    output->h = 16;
    output->clut = graphic.clut;
    output->tpage = GetTPage(0, 0, 0x380, 0x120);
}
