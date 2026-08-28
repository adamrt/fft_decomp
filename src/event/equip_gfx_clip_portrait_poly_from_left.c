#include "psx/gpu.h"
#include "psx/types.h"

void equip_gfx_clip_portrait_poly_from_left(POLY_FT4* poly, s32 amount) {
    /* x0/x2 are s16 in POLY_FT4 but the target loads them lhu. */
    *(u16*)&poly->x0 += amount;
    *(u16*)&poly->x2 += amount;
    if (poly->u0 != poly->u1) {
        poly->u1 -= amount;
        poly->u3 -= amount;
    } else {
        poly->v1 -= amount;
        poly->v3 -= amount;
    }
}
