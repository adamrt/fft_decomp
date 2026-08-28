#include "fft/bunit.h"
#include "psx/types.h"

void bunit_gfx_enqueue_draw_mode(s32 dfe, s32 dtd, s32 tpage, RECT* tw, s32 idx) {
    DR_MODE* primitive;

    primitive = &g_bunit_gfx_context->draw_modes[g_bunit_gfx_draw_mode_count++];
    SetDrawMode(primitive, dfe, dtd, tpage, tw);
    {
        bunit_gfx_context_t* db = g_bunit_gfx_context;
        setaddr(primitive, getaddr(&db->otag[idx]));
        setaddr(&db->otag[idx], primitive);
    }
}
