#include "fft/main.h"
#include "psx/gte.h"

void main_gfx_build_zodiac_sprite_polygon(POLY_FT4* poly, main_zodiac_texture_position_t* texture,
    main_zodiac_screen_position_t* base, main_zodiac_sprite_frame_t* frame, main_zodiac_scale_t* scale,
    main_zodiac_draw_offset_t* offset) {
    s32 x = frame->offset_x * scale->x;
    s32 y = frame->offset_y * scale->y;
    s32 width = frame->width * scale->x;
    s32 height = frame->height * scale->y;
    s32 round_x = 0;
    s32 round_y = 0;
    s32 round_width = 0;
    s32 round_height = 0;
    s32 qx;
    s32 qy;
    s32 qw;
    s32 qh;
    s32 right;
    s32 bottom;

    qx = x / ONE;
    if (x - qx * ONE >= 0x800) {
        round_x = 1;
    }
    qy = y / ONE;
    if (y - qy * ONE >= 0x800) {
        round_y = 1;
    }
    qw = width / ONE;
    if (width - qw * ONE >= 0x800) {
        round_width = 1;
    }
    qh = height / ONE;
    if (height - qh * ONE >= 0x800) {
        round_height = 1;
    }
    x = qx + round_x;
    y = qy + round_y;
    width = qw + round_width;
    height = qh + round_height;
    if (width < 0) {
        width = -width;
    }
    if (height < 0) {
        height = -height;
    }
    poly->tpage = GetTPage(1, 0, texture->x, texture->y & 0xF00);
    poly->u0 = frame->u;
    poly->v0 = frame->v;
    /* The target adds only the low byte of the 16-bit width/height here. */
    poly->u1 = frame->u + (u8)frame->width;
    poly->v1 = frame->v;
    poly->u2 = frame->u;
    poly->v2 = frame->v + (u8)frame->height;
    poly->u3 = frame->u + (u8)frame->width;
    poly->v3 = frame->v + (u8)frame->height;
    right = width + 0x100;
    bottom = height + 0x78;
    poly->x0 = base->x + x + offset->x + 0x100;
    poly->y0 = base->y + y + offset->y + 0x78;
    poly->x1 = base->x + x + offset->x + right;
    poly->y1 = base->y + y + offset->y + 0x78;
    poly->x2 = base->x + x + offset->x + 0x100;
    poly->y2 = base->y + y + offset->y + bottom;
    poly->x3 = base->x + x + offset->x + right;
    poly->y3 = base->y + y + offset->y + bottom;
}
