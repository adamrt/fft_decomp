/* Queuing a draw mode packet at an ordering-table entry. */

#include "fft/menu.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Takes the next DR_MODE packet from the pool, fills it and links it into
 * ordering-table entry idx. */
void menu_gfx_enqueue_draw_mode(s32 dfe, s32 dtd, s32 tpage, RECT* texture_window, s32 otag_index) {
    DR_MODE* mode = &g_menu_gfx_context->draw_modes[g_menu_gfx_draw_mode_count++];

    SetDrawMode(mode, dfe, dtd, tpage, texture_window);
    addPrim(&g_menu_gfx_context->otag[otag_index], mode);
}
