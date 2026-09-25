#include "fft/event_attack.h"

/* Configure a sprite quad using truncated 4.12 fixed-point geometry. */
void attack_gfx_set_scaled_poly_ft4_geometry(POLY_FT4* poly, attack_gfx_texture_page_position_t* texture_page,
    attack_gfx_point_t* position, attack_gfx_sprite_rect_t* sprite_rect, attack_gfx_point_t* scale, POLY_FT4* base) {
    s16 scale_x;
    s16 scale_y;
    s32 dx;
    s32 dy;
    s32 w;
    s32 h;

    scale_x = scale->x;
    dx = (sprite_rect->offset_x * scale_x) / ONE;
    scale_y = scale->y;
    dy = (sprite_rect->offset_y * scale_y) / ONE;
    w = (sprite_rect->width * scale_x) / ONE;
    h = (sprite_rect->height * scale_y) / ONE;
    if (w < 0) {
        w = -w;
    }
    if (h < 0) {
        h = -h;
    }

    poly->tpage = GetTPage(0, 0, texture_page->x, texture_page->y & 0xF00);
    poly->u0 = sprite_rect->u;
    poly->v0 = sprite_rect->v;
    poly->u1 = sprite_rect->u + sprite_rect->width;
    poly->v1 = sprite_rect->v;
    poly->u2 = sprite_rect->u;
    poly->v2 = sprite_rect->v + sprite_rect->height;
    poly->u3 = sprite_rect->u + sprite_rect->width;
    poly->v3 = sprite_rect->v + sprite_rect->height;
    poly->x0 = position->x + dx + base->x0;
    poly->y0 = position->y + dy + base->y0;
    poly->x1 = position->x + dx + base->x0 + w;
    poly->y1 = position->y + dy + base->y0;
    poly->x2 = position->x + dx + base->x0;
    poly->y2 = position->y + dy + base->y0 + h;
    poly->x3 = position->x + dx + base->x0 + w;
    poly->y3 = position->y + dy + base->y0 + h;
}
