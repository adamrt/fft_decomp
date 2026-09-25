#include "fft/event_bunit.h"
#include "psx/types.h"

void bunit_gfx_enqueue_draw_area(RECT* rect, s32 idx) {
    DR_AREA* p;

    if (g_bunit_gfx_drawenv_y < 0x64) {
        rect->y += 0xF0;
    }

    p = &g_bunit_gfx_context->draw_areas[g_bunit_gfx_draw_area_count++];
    SetDrawArea(p, rect);
    {
        bunit_gfx_context_t* db = g_bunit_gfx_context;
        setaddr(p, getaddr(&db->otag[idx]));
        setaddr(&db->otag[idx], p);
    }
}
