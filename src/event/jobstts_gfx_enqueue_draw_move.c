#include "fft/event_jobstts.h"

void jobstts_gfx_enqueue_draw_move(const RECT* rect, s32 u, s32 v, s32 otag_index) {
    DR_MOVE* prim;
    const RECT* src = rect;
    u16 index = g_jobstts_gfx_draw_move_count;

    g_jobstts_gfx_draw_move_count = index + 1;
    prim = &g_jobstts_gfx_context->draw_moves[index];
    SetDrawMove(prim);
    prim->x0 = src->x;
    prim->y0 = src->y;
    prim->x1 = u;
    prim->y1 = v;
    prim->w = src->w;
    prim->h = src->h;
    {
        jobstts_gfx_context_t* context = g_jobstts_gfx_context;
        setaddr(prim, getaddr(&context->otag[otag_index]));
        setaddr(&context->otag[otag_index], prim);
    }
}
