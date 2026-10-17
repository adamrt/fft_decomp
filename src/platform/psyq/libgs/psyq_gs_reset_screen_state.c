#include "psx/libgpu.h"
#include "psx/libgs.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Screen-state reset shared by GsInitGraph and GsInitGraph2: records the
 * screen size, builds the identity and its aspect-scaled copy, zeroes the two
 * light matrices and the draw offsets, sets the clip rectangle and primes the
 * clear tiles. */
void psyq_gs_reset_screen_state(u16 width, u16 height) {
    VWD0 = height;
    HWD0 = width;
    GsIDMATRIX.m[0][2] = 0;
    GsIDMATRIX.m[0][1] = 0;
    GsIDMATRIX.m[1][2] = 0;
    GsIDMATRIX.m[1][0] = 0;
    GsIDMATRIX.m[2][1] = 0;
    GsIDMATRIX.m[2][0] = 0;
    GsIDMATRIX.t[2] = 0;
    GsIDMATRIX.t[1] = 0;
    GsIDMATRIX.t[0] = 0;
    GsIDMATRIX.m[0][0] = ONE;
    GsIDMATRIX.m[1][1] = ONE;
    GsIDMATRIX.m[2][2] = ONE;
    GsIDMATRIX2 = GsIDMATRIX;
    GsLIGHTWSMATRIX = GsIDMATRIX;
    GsLIGHTWSMATRIX.m[2][2] = 0;
    GsLIGHTWSMATRIX.m[1][1] = 0;
    GsLIGHTWSMATRIX.m[0][0] = 0;
    _LC = GsLIGHTWSMATRIX;
    PSDBASEX[0] = 0;
    PSDBASEX[1] = 0;
    PSDBASEY[0] = 0;
    PSDBASEY[1] = 0;
    POSITION.offy = 0;
    POSITION.offx = 0;
    CLIP2.y = 0;
    GsIDMATRIX2.m[1][1] = (VWD0 << 14) / HWD0 / 3;
    CLIP2.x = 0;
    CLIP2.w = HWD0;
    CLIP2.h = VWD0;
    SetBlockFill(&g_psyq_gs_clear_tile[0]);
    SetBlockFill(&g_psyq_gs_clear_tile[1]);
    PSDCNT = 1;
}
