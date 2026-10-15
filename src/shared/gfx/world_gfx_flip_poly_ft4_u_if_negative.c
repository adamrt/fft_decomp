#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Mirror a POLY_FT4 texture horizontally when direction is negative: u0/u1
 * and u2/u3 trade places, each shifted by one texel. */
void world_gfx_flip_poly_ft4_u_if_negative(POLY_FT4* poly, s32 direction) {
    s32 step;
    u8 u;

    if (direction >= 0) {
        return;
    }
    if (poly->u0 < poly->u1) {
        step = -1;
    } else {
        step = 1;
    }
    u = poly->u0 + step;
    poly->u0 = poly->u1 + step;
    poly->u1 = u;
    u = poly->u2 + step;
    poly->u2 = poly->u3 + step;
    poly->u3 = u;
}
