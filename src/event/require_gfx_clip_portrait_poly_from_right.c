#include "psx/gpu.h"
#include "psx/types.h"

void require_gfx_clip_portrait_poly_from_right(POLY_FT4* poly, s32 amount) {
    if (poly->u0 != poly->u1) {
        poly->u0 += amount;
        poly->u2 += amount;
    } else {
        poly->v0 += amount;
        poly->v2 += amount;
    }
    /* x1/x3 are s16 in POLY_FT4 but the target loads them lhu. */
    *(u16*)&poly->x1 -= amount;
    *(u16*)&poly->x3 -= amount;
}
