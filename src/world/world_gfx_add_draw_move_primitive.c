#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Takes the next DR_MOVE packet from the pool, sets it up to copy `src`
 * to (x, y) and links it into ordering-table entry ot_index (libgpu addPrim). */
void world_gfx_add_draw_move_primitive(RECT* src, s32 x, s32 y, s32 ot_index) {
    DR_MOVE* move = &g_world_gfx_active_packet_buffer->draw_moves[g_world_gfx_draw_move_count++];

    SetDrawMove(move);
    move->x0 = src->x;
    move->y0 = src->y;
    move->x1 = x;
    move->y1 = y;
    move->w = src->w;
    move->h = src->h;
    setaddr(move, getaddr(&g_world_gfx_active_packet_buffer->otag[ot_index]));
    setaddr(&g_world_gfx_active_packet_buffer->otag[ot_index], move);
}
