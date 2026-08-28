#include "psx/gpu.h"
#include "psx/types.h"

void battle_gfx_flip_poly_ft4_uv_if_negative(POLY_FT4* primitive, s32 direction) {
    u8 temp;

    if (direction < 0) {
        if (primitive->u0 != primitive->u1) {
            temp = primitive->u0;
            primitive->u0 = primitive->u1;
            primitive->u1 = temp;
            temp = primitive->u2;
            primitive->u2 = primitive->u3;
            primitive->u3 = temp;
        } else {
            temp = primitive->v0;
            primitive->v0 = primitive->v1;
            primitive->v1 = temp;
            temp = primitive->v2;
            primitive->v2 = primitive->v3;
            primitive->v3 = temp;
        }
    }
}
