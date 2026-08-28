#include "psx/gpu.h"
#include "psx/types.h"

void attack_gfx_set_formation_icon_uv(POLY_FT4* poly, s32 index) {
    s32 u = (index % 7) * 0x20;
    s32 v = (index / 7) * 0x30;

    poly->u0 = u;
    poly->v0 = v + 0x28;
    poly->u1 = u + 0x1f;
    poly->v1 = v + 0x28;
    poly->u2 = u;
    poly->v2 = v + 0x58;
    poly->u3 = u + 0x1f;
    poly->v3 = v + 0x58;
    poly->tpage = GetTPage(0, 0, 0x100, 0);
    poly->clut = GetClut(((index % 3) << 4) + 0x100, index / 3);
}
