/* Queuing the context's single draw mode packet at an ordering-table entry. */

#include "fft/event_card.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Fill the context's single DR_MODE packet and link it into ordering-table
 * entry otag_index. */
void menu_gfx_enqueue_draw_mode_variant(s32 dfe, s32 dtd, s32 tpage, RECT* texture_window, s32 otag_index) {
    card_graphics_context_t* context = g_card_gfx_context;
    DR_MODE* mode = &context->draw_mode;

    SetDrawMode(mode, dfe, dtd, tpage, texture_window);
    addPrim(&g_card_gfx_context->otag[otag_index], mode);
}
