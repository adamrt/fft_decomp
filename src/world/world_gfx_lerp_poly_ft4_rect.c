#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Interpolate the four corners of a textured quad between two RECTs. */
void world_gfx_lerp_poly_ft4_rect(POLY_FT4* poly, RECT* from, RECT* to, s32 t) {
    poly->x0 = world_menu_lerp_fixed12(from->x, to->x, t);
    poly->y0 = world_menu_lerp_fixed12(from->y, to->y, t);
    poly->x1 = world_menu_lerp_fixed12(from->x + from->w, to->x + to->w, t);
    poly->y1 = world_menu_lerp_fixed12(from->y, to->y, t);
    poly->x2 = world_menu_lerp_fixed12(from->x, to->x, t);
    poly->y2 = world_menu_lerp_fixed12(from->y + from->h, to->y + to->h, t);
    poly->x3 = world_menu_lerp_fixed12(from->x + from->w, to->x + to->w, t);
    poly->y3 = world_menu_lerp_fixed12(from->y + from->h, to->y + to->h, t);
}
