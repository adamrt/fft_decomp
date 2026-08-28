#include "psx/gpu.h"

void attack_gfx_clip_portrait_poly_from_left(POLY_FT4* poly, s32 amount) {
    poly->x0 += amount;
    poly->x2 += amount;
    if (poly->u0 != poly->u1) {
        poly->u1 -= amount;
        poly->u3 -= amount;
    } else {
        poly->v1 -= amount;
        poly->v3 -= amount;
    }
}
