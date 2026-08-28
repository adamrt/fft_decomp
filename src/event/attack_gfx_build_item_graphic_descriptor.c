#include "fft/battle.h"
#include "fft/status_panel.h"
#include "psx/gpu.h"
#include "psx/types.h"

void attack_gfx_build_item_graphic_descriptor(status_panel_graphic_descriptor_t* output, u32 item_id) {
    SPRT graphic;

    battle_get_item_graphic_data(&graphic, item_id);
    output->x_load_location = graphic.u0;
    output->y_load_location = graphic.v0;
    output->width = 16;
    output->height = 16;
    output->clut = graphic.clut;
    output->tpage = GetTPage(0, 0, 0x380, 0x120);
}
