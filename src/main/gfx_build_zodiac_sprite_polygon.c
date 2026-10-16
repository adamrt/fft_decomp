#include "fft/gfx.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Build a centered sprite-cell quad using rounded 4.12 fixed-point geometry.
 *
 * This variant selects 8-bit texture mode and adds the PSX screen center to
 * the draw offset. */
void gfx_build_zodiac_sprite_polygon(POLY_FT4* poly, point16_t* texture_page, const RECT* position,
    sprite_sheet_frame_t* sprite_rect, point16_t* scale, const sprite_draw_offset_t* offset) {
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

    ox = sprite_rect->offset.x * scale->x;
    oy = sprite_rect->offset.y * scale->y;
    w = sprite_rect->source.w * scale->x;
    h = sprite_rect->source.h * scale->y;

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
    setUV4(poly, sprite_rect->source.x, sprite_rect->source.y, sprite_rect->source.x + sprite_rect->source.w,
        sprite_rect->source.y, sprite_rect->source.x, sprite_rect->source.y + sprite_rect->source.h,
        sprite_rect->source.x + sprite_rect->source.w, sprite_rect->source.y + sprite_rect->source.h);

    x1 = w + 0x100;
    y1 = h + 0x78;

    setXY4(poly, position->x + ox + offset->position.x + 0x100, position->y + oy + offset->position.y + 0x78,
        position->x + ox + offset->position.x + x1, position->y + oy + offset->position.y + 0x78,
        position->x + ox + offset->position.x + 0x100, position->y + oy + offset->position.y + y1,
        position->x + ox + offset->position.x + x1, position->y + oy + offset->position.y + y1);
}
