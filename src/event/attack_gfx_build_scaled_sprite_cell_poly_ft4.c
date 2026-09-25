#include "fft/event_attack.h"

/* Build a centered sprite-cell quad using rounded 4.12 fixed-point geometry.
 *
 * This variant selects 8-bit texture mode and adds the PSX screen center to
 * the draw offset (battle_menu_status_panel_scaled_sprite_t.offset[4..5]). */
void attack_gfx_build_scaled_sprite_cell_poly_ft4(POLY_FT4* poly, attack_gfx_texture_page_position_t* texture_page,
    const RECT* position, attack_gfx_sprite_rect_t* sprite_rect, attack_gfx_point_t* scale, const s16* offset) {
    s32 rx;
    s32 ry;
    s32 rw;
    s32 rh;
    s32 ox;
    s32 oy;
    s32 w;
    s32 h;
    s32 x1;
    s32 y1;

    ox = sprite_rect->offset_x * scale->x;
    oy = sprite_rect->offset_y * scale->y;
    w = sprite_rect->width * scale->x;
    h = sprite_rect->height * scale->y;

    rx = 0;
    ry = 0;
    rw = 0;
    rh = 0;
    if ((ox - ((ox / ONE) * ONE)) >= 0x800) {
        rx = 1;
    }
    if ((oy - ((oy / ONE) * ONE)) >= 0x800) {
        ry = 1;
    }
    if ((w - ((w / ONE) * ONE)) >= 0x800) {
        rw = 1;
    }
    if ((h - ((h / ONE) * ONE)) >= 0x800) {
        rh = 1;
    }

    ox = (ox / ONE) + rx;
    oy = (oy / ONE) + ry;
    w = (w / ONE) + rw;
    h = (h / ONE) + rh;
    if (w < 0) {
        w = -w;
    }
    if (h < 0) {
        h = -h;
    }

    poly->tpage = GetTPage(1, 0, texture_page->x, texture_page->y & 0xF00);
    poly->u0 = sprite_rect->u;
    poly->v0 = sprite_rect->v;
    poly->u1 = sprite_rect->u + (u8)sprite_rect->width;
    poly->v1 = sprite_rect->v;
    poly->u2 = sprite_rect->u;
    poly->v2 = sprite_rect->v + (u8)sprite_rect->height;
    poly->u3 = sprite_rect->u + (u8)sprite_rect->width;
    poly->v3 = sprite_rect->v + (u8)sprite_rect->height;

    x1 = w + 0x100;
    y1 = h + 0x78;

    poly->x0 = position->x + ox + offset[4] + 0x100;
    poly->y0 = position->y + oy + offset[5] + 0x78;
    poly->x1 = position->x + ox + offset[4] + x1;
    poly->y1 = position->y + oy + offset[5] + 0x78;
    poly->x2 = position->x + ox + offset[4] + 0x100;
    poly->y2 = position->y + oy + offset[5] + y1;
    poly->x3 = position->x + ox + offset[4] + x1;
    poly->y3 = position->y + oy + offset[5] + y1;
}
