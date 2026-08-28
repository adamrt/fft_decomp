#include "fft/bunit.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Shift a portrait quad's left edge right by `amount` and trim the same amount
 * from its right-hand texture coordinates: u when the texture runs along u,
 * otherwise v. */
void bunit_gfx_clip_portrait_poly_from_left(POLY_FT4* poly, s32 amount) {
    poly->x0 = poly->x0 + amount;
    poly->x2 = poly->x2 + amount;
    if (poly->u0 != poly->u1) {
        poly->u1 = poly->u1 - amount;
        poly->u3 = poly->u3 - amount;
        return;
    }
    poly->v1 = poly->v1 - amount;
    poly->v3 = poly->v3 - amount;
}
