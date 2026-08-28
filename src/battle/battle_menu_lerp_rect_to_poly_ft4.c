#include "psx/gpu.h"
#include "psx/types.h"

/* Set a quad's corners to the rectangle interpolated between from and to by a 12-bit fraction. */
void battle_menu_lerp_rect_to_poly_ft4(POLY_FT4* poly, RECT* from, RECT* to, s32 factor) {
    poly->x0 = battle_menu_lerp_fixed12(from->x, to->x, factor);
    poly->y0 = battle_menu_lerp_fixed12(from->y, to->y, factor);
    poly->x1 = battle_menu_lerp_fixed12(from->x + from->w, to->x + to->w, factor);
    poly->y1 = battle_menu_lerp_fixed12(from->y, to->y, factor);
    poly->x2 = battle_menu_lerp_fixed12(from->x, to->x, factor);
    poly->y2 = battle_menu_lerp_fixed12(from->y + from->h, to->y + to->h, factor);
    poly->x3 = battle_menu_lerp_fixed12(from->x + from->w, to->x + to->w, factor);
    poly->y3 = battle_menu_lerp_fixed12(from->y + from->h, to->y + to->h, factor);
}
