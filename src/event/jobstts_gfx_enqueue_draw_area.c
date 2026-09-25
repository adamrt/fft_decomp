#include "fft/event_jobstts.h"

/* Queue a drawing-area primitive, shifting it to the back buffer when needed. */
void jobstts_gfx_enqueue_draw_area(RECT* rect, s32 idx) {
    DR_AREA* p;

    if (g_jobstts_gfx_drawenv_y < 0x64) {
        rect->y += 0xF0;
    }

    p = &g_jobstts_gfx_context->draw_areas[g_jobstts_gfx_draw_area_count++];
    SetDrawArea(p, rect);
    {
        jobstts_gfx_context_t* context = g_jobstts_gfx_context;
        setaddr(p, getaddr(&context->otag[idx]));
        setaddr(&context->otag[idx], p);
    }
}
