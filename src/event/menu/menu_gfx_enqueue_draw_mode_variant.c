#include "fft/menu.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Takes the next DR_MODE packet from the pool, fills it and links it into
 * ordering-table entry idx. */
void menu_gfx_enqueue_draw_mode_variant(s32 dfe, s32 dtd, s32 tpage, RECT* tw, s32 idx) {
    DR_MODE* primitive;

    primitive = &g_menu_gfx_context->draw_modes[g_menu_gfx_draw_mode_count++];
    SetDrawMode(primitive, dfe, dtd, tpage, tw);
    addPrim(&g_menu_gfx_context->otag[idx], primitive);
}
