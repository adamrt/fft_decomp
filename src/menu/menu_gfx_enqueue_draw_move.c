#include "fft/menu.h"

/* Takes the next DR_MOVE packet from the pool, sets it up to copy `rect`
 * to (u, v) and links it into ordering-table entry otag_index. */
void menu_gfx_enqueue_draw_move(const RECT* rect, s32 u, s32 v, s32 otag_index) {
    DR_MOVE* prim;

    prim = &g_menu_gfx_context->draw_moves[g_menu_gfx_draw_move_count++];
    SetDrawMove(prim);
    prim->sx = rect->x;
    prim->sy = rect->y;
    prim->x0 = u;
    prim->y0 = v;
    prim->w = rect->w;
    prim->h = rect->h;
    addPrim(&g_menu_gfx_context->otag[otag_index], prim);
}
