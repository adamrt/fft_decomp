#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Mirror a POLY_FT4 texture horizontally when direction is negative: u0/u1
 * and u2/u3 trade places, each shifted by one texel.
 *
 * The per-role variables and the three empty asms (zero-instruction scheduling
 * boundaries) are what reproduce the target's load/store order. */
void world_gfx_flip_poly_ft4_u_if_negative(POLY_FT4* poly, s32 direction) {
    s32 step;
    u8 u0;
    u8 u1;
    u8 u2;
    u8 u3;
    u8 swapped;

    if (direction >= 0) {
        return;
    }
    if (poly->u0 < poly->u1) {
        step = -1;
    } else {
        step = 1;
    }
    u0 = poly->u0;
    __asm__ volatile("");
    u1 = poly->u1;
    u2 = poly->u2;
    __asm__ volatile("");
    swapped = u0 + step;
    poly->u1 = swapped;
    __asm__ volatile("");
    swapped = u2 + step;
    u3 = poly->u3;
    u1 += step;
    poly->u0 = u1;
    poly->u3 = swapped;
    u3 += step;
    poly->u2 = u3;
}
