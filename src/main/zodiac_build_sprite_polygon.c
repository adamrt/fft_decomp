/* A centered, scaled 8bpp sprite quad for the zodiac graphic. */

#include "fft/gfx.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Build an 8-bit textured quad for one sprite-sheet cell, scaled by scale
 * (4.12 fixed). The cell's offset and size are scaled and rounded to whole
 * pixels; the size is made positive, so a negative scale moves the offset but
 * does not mirror the cell. The quad sits at position + offset + the scaled
 * cell offset + (256, 120), the center of a 256x240 screen whose draw area
 * starts at x -128 as BATTLE's does. */
void zodiac_build_sprite_polygon(POLY_FT4* poly, point16_t* texture_page, const RECT* position,
    sprite_sheet_frame_t* sprite_rect, point16_t* scale, const sprite_draw_offset_t* offset) {
    s32 rx = 0;
    s32 ry = 0;
    s32 rw = 0;
    s32 rh = 0;
    s32 ox = sprite_rect->offset.x * scale->x;
    s32 oy = sprite_rect->offset.y * scale->y;
    s32 w = sprite_rect->source.w * scale->x;
    s32 h = sprite_rect->source.h * scale->y;
    s32 x1;
    s32 y1;

    /* Round each product half up. A negative product's remainder is negative, so it
     * truncates toward zero instead. */
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
