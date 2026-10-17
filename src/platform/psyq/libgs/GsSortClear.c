#include "psx/libgpu.h"
#include "psx/libgs.h"
#include "psx/types.h"

/* Register a full-screen clear tile for the active buffer. */
void GsSortClear(u8 r, u8 g, u8 b, GsOT* otp) {
    s32 i;
    s16 x;
    s16 y;
    s16 h;

    g_psyq_gs_clear_tile[PSDIDX].r0 = r;
    g_psyq_gs_clear_tile[PSDIDX].g0 = g;
    g_psyq_gs_clear_tile[PSDIDX].b0 = b;
    i = PSDIDX;
    x = PSDOFSX[i];
    h = VWD0;
    g_psyq_gs_clear_tile[i].x0 = x;
    y = PSDOFSY[i];
    g_psyq_gs_clear_tile[i].h = h;
    g_psyq_gs_clear_tile[i].y0 = y;
    if (GsDISPENV.isrgb24 != 0) {
        g_psyq_gs_clear_tile[i].w = HWD0 * 3 / 2;
    } else {
        g_psyq_gs_clear_tile[i].w = HWD0;
    }
    AddPrim(otp->tag, &g_psyq_gs_clear_tile[PSDIDX]);
}
