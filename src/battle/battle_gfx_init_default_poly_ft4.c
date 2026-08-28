#include "psx/gpu.h"
#include "psx/types.h"

void battle_gfx_init_default_poly_ft4(POLY_FT4* poly) {
    SetPolyFT4(poly);
    SetShadeTex(poly, 0);
    poly->r0 = 0x80;
    poly->g0 = 0x80;
    poly->b0 = 0x80;
    poly->u1 = 1;
    poly->v2 = 1;
    poly->u3 = 1;
    poly->v3 = 1;
    poly->u0 = 0;
    poly->v0 = 0;
    poly->v1 = 0;
    poly->u2 = 0;
    poly->x0 = -0x200;
    poly->y0 = 0;
    poly->x1 = -0x200;
    poly->y1 = 0;
    poly->x2 = -0x200;
    poly->y2 = 0;
    poly->x3 = -0x200;
    poly->y3 = 0;
    poly->tpage = GetTPage(0, 0, 0x1C0, 0);
    poly->clut = 0x7C3C;
}
