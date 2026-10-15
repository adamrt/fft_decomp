#include "fft/event_etc.h"
#include "fft/gfx.h"
#include "psx/types.h"

/* The dimension row of the tile being built. Spelled out at each access: the
 * target forms this address inside every memory reference (scaled terms
 * first), which a row pointer variable does not. */
#define ROW (dimensions + dimension_set + tile)

/* Build and draw the four POLY_GT4 tiles of an ETC graphic revealed up to `fade`
 * pixels, followed by a gradient edge of up to 32 pixels.
 *
 * The graphic is always 8-bit (texture mode 1). Retail keeps the first
 * half's right edge in the dead `graphic_id` argument. The unsigned vertical
 * origin preserves the width/x load order. */
void etc_graphic_build_game_over_polygons(
    s32 graphic_id, s32 fade, s32 dimension_set, s32 reverse_order, POLY_GT4* primitives, s32 color) {
    sprite_sheet_frame_t* dimensions;
    POLY_GT4* primitive;
    s32 texture_mode;
    s32 half;
    s32 tile;
    s32 clip;
    s32 fade_x;

    texture_mode = 1;
    dimensions = g_etc_graphics[graphic_id].dimensions;
    primitive = primitives;
    for (half = 0; half < 2; half++) {
        for (tile = 0; tile < 2; tile++) {
            SetPolyGT4(primitive);
            SetShadeTex(primitive, 0);
            SetSemiTrans(primitive, 1);
            setUV4(primitive, 0, 0, 1, 0, 0, 1, 1, 1);
            setXY4(primitive, 0, 0, 0, 0, 0, 0, 0, 0);
            if (tile != 0) {
                primitive->tpage = getTPage(texture_mode, 1, 384, 0);
            } else {
                primitive->tpage = getTPage(texture_mode, 2, 384, 0);
            }
            if (texture_mode != 0) {
                primitive->clut = 0x7f80;
            } else {
                primitive->clut = 0xfdb;
            }

            if (half == 0) {
                if (reverse_order == 0) {
                    primitive->u0 = ROW->source.x + ROW->source.w;
                    primitive->v0 = ROW->source.y;
                    primitive->u2 = ROW->source.x;
                    primitive->v2 = ROW->source.y;
                    primitive->u1 = ROW->source.x + ROW->source.w;
                    primitive->v1 = ROW->source.y + fade;
                    primitive->u3 = ROW->source.x;
                    primitive->v3 = ROW->source.y + fade;
                    primitive->x0 = ROW->offset.y + 0x100;
                    primitive->y0 = ROW->offset.x + 0x80;
                    graphic_id = fade + 0x100;
                    primitive->x1 = ROW->offset.y + graphic_id;
                    primitive->y1 = ROW->offset.x + 0x80;
                    primitive->x2 = ROW->offset.y + 0x100;
                    primitive->y2 = ROW->offset.x + (ROW->source.w + 0x80U);
                    primitive->x3 = ROW->offset.y + graphic_id;
                    primitive->y3 = ROW->offset.x + (ROW->source.w + 0x80U);
                    setRGB0(primitive, color, color, color);
                    setRGB1(primitive, color, color, color);
                    setRGB2(primitive, color, color, color);
                    setRGB3(primitive, color, color, color);
                }
            } else if (half == 1) {
                clip = ROW->source.h < fade + 0x20 ? ROW->source.h - fade : 0x20;
                if (reverse_order == 0) {
                    fade_x = fade + 0x100;
                    primitive->u0 = ROW->source.x + ROW->source.w;
                    primitive->v0 = ROW->source.y + fade;
                    primitive->u2 = ROW->source.x;
                    primitive->v2 = ROW->source.y + fade;
                    primitive->u1 = ROW->source.x + ROW->source.w;
                    primitive->v1 = ROW->source.y + fade + clip;
                    primitive->u3 = ROW->source.x;
                    primitive->v3 = ROW->source.y + fade + clip;
                    setXY4(primitive, ROW->offset.y + fade_x, ROW->offset.x + 0x80, ROW->offset.y + fade_x + clip,
                        ROW->offset.x + 0x80, ROW->offset.y + fade_x, ROW->offset.x + (ROW->source.w + 0x80U),
                        ROW->offset.y + fade_x + clip, ROW->offset.x + (ROW->source.w + 0x80U));
                    setRGB0(primitive, color, color, color);
                    setRGB1(primitive, 0, 0, 0);
                    setRGB2(primitive, color, color, color);
                    setRGB3(primitive, 0, 0, 0);
                }
            }
            primitive++;
        }
    }

    if (reverse_order != 0) {
        gfx_draw_or_append_gpu_primitive(&primitives[1]);
        gfx_draw_or_append_gpu_primitive(&primitives[0]);
        gfx_draw_or_append_gpu_primitive(&primitives[3]);
        gfx_draw_or_append_gpu_primitive(&primitives[2]);
    } else {
        gfx_draw_or_append_gpu_primitive(&primitives[3]);
        gfx_draw_or_append_gpu_primitive(&primitives[2]);
        gfx_draw_or_append_gpu_primitive(&primitives[1]);
        gfx_draw_or_append_gpu_primitive(&primitives[0]);
    }
}
