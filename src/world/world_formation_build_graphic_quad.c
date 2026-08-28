#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Fills one POLY_FT4 for a formation graphic entry: scales the entry's
 * offset and size by a 12-bit fixed scale (rounded to the nearest pixel,
 * size made positive) and places it at position + origin. */
void world_formation_build_graphic_quad(POLY_FT4* poly, world_formation_texture_page_location_t* page,
    world_formation_screen_point_t* position, world_formation_graphic_rect_t* rect,
    world_formation_fixed_scale_t* scale, world_formation_sprite_origin_t* origin) {
    s32 x_off = rect->x_off * scale->x;
    s32 y_off = rect->y_off * scale->y;
    s32 w = rect->w * scale->x;
    s32 h = rect->h * scale->y;
    s32 round_x = 0;
    s32 round_y = 0;
    s32 round_w = 0;
    s32 round_h = 0;
    s32 whole_x;
    s32 whole_y;
    s32 whole_w;
    s32 whole_h;

    whole_x = x_off / ONE;
    if (x_off - whole_x * ONE >= 0x800) {
        round_x = 1;
    }
    whole_y = y_off / ONE;
    if (y_off - whole_y * ONE >= 0x800) {
        round_y = 1;
    }
    whole_w = w / ONE;
    if (w - whole_w * ONE >= 0x800) {
        round_w = 1;
    }
    whole_h = h / ONE;
    if (h - whole_h * ONE >= 0x800) {
        round_h = 1;
    }
    x_off = whole_x + round_x;
    y_off = whole_y + round_y;
    w = whole_w + round_w;
    h = whole_h + round_h;
    if (w < 0) {
        w = -w;
    }
    if (h < 0) {
        h = -h;
    }
    poly->tpage = GetTPage(1, 0, page->x, page->y & 0xF00);
    poly->u0 = rect->u;
    poly->v0 = rect->v;
    poly->u1 = rect->u + rect->w;
    poly->v1 = rect->v;
    poly->u2 = rect->u;
    poly->v2 = rect->v + rect->h;
    poly->u3 = rect->u + rect->w;
    poly->v3 = rect->v + rect->h;
    poly->x0 = position->x + x_off + origin->x;
    poly->y0 = position->y + y_off + origin->y;
    poly->x1 = position->x + x_off + origin->x + w;
    poly->y1 = position->y + y_off + origin->y;
    poly->x2 = position->x + x_off + origin->x;
    poly->y2 = position->y + y_off + origin->y + h;
    poly->x3 = position->x + x_off + origin->x + w;
    poly->y3 = position->y + y_off + origin->y + h;
}
