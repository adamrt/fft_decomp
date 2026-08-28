#include "fft/card.h"
#include "psx/gpu.h"
#include "psx/types.h"

void card_gfx_set_draw_mode_and_link_ot(s32 dfe, s32 dtd, s32 tpage, RECT* texture_window, s32 otag_index) {
    card_graphics_context_t* context = g_card_gfx_context;
    DR_MODE* mode = &context->draw_mode;

    SetDrawMode(mode, dfe, dtd, tpage, texture_window);
    {
        card_graphics_context_t* link_context = g_card_gfx_context;
        setaddr(mode, getaddr(&link_context->otag[otag_index]));
        setaddr(&link_context->otag[otag_index], mode);
    }
}
