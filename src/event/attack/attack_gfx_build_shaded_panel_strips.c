#include "fft/event_attack.h"
#include "fft/gfx.h"

/*
 * Builds and submits a 2x2 set of shaded POLY_GT4 panel strips at the current
 * thread's position.
 *
 * Row 0 is the solid part of the strip and row 1 a narrow edge whose shade
 * fades to 0; `reverse` selects which side of `width` is drawn. Column 0 is
 * the same quad offset by one pixel with a different blend mode.
 *
 * `height` is a local rather than a literal so the narrowed y2/y3 sums reuse
 * the y0 sum (a literal lets fold reassociate them). `rest` is assigned inside
 * its branch: that keeps the target's loop hoisting, where
 * `x + (0x80 + width)` is recomputed once per row instead of leaving both loops.
 */
void attack_gfx_build_shaded_panel_strips(POLY_GT4* polys, s32 width, s32 shade, s32 reverse) {
    POLY_GT4* poly;
    s32 row;
    s32 rest;
    s32 x;
    s32 y;
    s32 col;
    s32 dx;
    s32 dy;
    s32 edge;
    s32 height;

    x = thread_get_current_parameter_1_variant();
    y = thread_get_current_parameter_2_variant();
    poly = polys;
    height = 0x14;
    for (row = 0; row < 2; row++) {
        for (col = 0; col < 2; col++) {
            SetPolyGT4(poly);
            SetShadeTex(poly, 0);
            SetSemiTrans(poly, 1);
            if (col != 0) {
                if (GetGraphType() == 1 || GetGraphType() == 2) {
                    poly->tpage = 0x87;
                } else {
                    poly->tpage = 0x27;
                }
                dx = 0;
                dy = 0;
            } else {
                if (GetGraphType() == 1 || GetGraphType() == 2) {
                    poly->tpage = 0x107;
                } else {
                    poly->tpage = 0x47;
                }
                dx = 1;
                dy = 1;
            }
            poly->clut = 0x3fdc;
            if (row == 0) {
                if (reverse == 0) {
                    setUVWH(poly, 0, 0xe0, width, 0x14);
                    setXYWH(poly, x + 0x80 + dx, y + 0x60 + dy, width, height);
                } else {
                    rest = 0xff - width;
                    setUVWH(poly, width, 0xe0, rest, 0x14);
                    setXYWH(poly, x + (0x80 + width) + dx, y + 0x60 + dy, rest, height);
                }
                setRGB0(poly, shade, shade, shade);
                setRGB1(poly, shade, shade, shade);
                setRGB2(poly, shade, shade, shade);
                setRGB3(poly, shade, shade, shade);
            } else {
                if (reverse == 0) {
                    setRGB0(poly, shade, shade, shade);
                    setRGB1(poly, 0, 0, 0);
                    setRGB2(poly, shade, shade, shade);
                    setRGB3(poly, 0, 0, 0);
                } else {
                    setRGB1(poly, shade, shade, shade);
                    setRGB0(poly, 0, 0, 0);
                    setRGB3(poly, shade, shade, shade);
                    setRGB2(poly, 0, 0, 0);
                }
                if (reverse == 0) {
                    edge = 0x20;
                    if (width >= 0xdf) {
                        edge = 0xfe - width;
                    }
                    setUVWH(poly, width, 0xe0, edge, 0x14);
                    setXYWH(poly, x + 0x80 + width + dx, y + 0x60 + dy, edge, height);
                } else {
                    edge = 0x20;
                    if (width < 0x20) {
                        edge = width;
                    }
                    setUVWH(poly, width - edge, 0xe0, edge, 0x14);
                    setXYWH(poly, x + (0x80 + width) - edge + dx, y + 0x60 + dy, edge, height);
                }
            }
            poly++;
        }
    }
    if (reverse != 0) {
        gfx_draw_or_append_gpu_primitive(&polys[1]);
        gfx_draw_or_append_gpu_primitive(&polys[0]);
        gfx_draw_or_append_gpu_primitive(&polys[3]);
        gfx_draw_or_append_gpu_primitive(&polys[2]);
    } else {
        gfx_draw_or_append_gpu_primitive(&polys[3]);
        gfx_draw_or_append_gpu_primitive(&polys[2]);
        gfx_draw_or_append_gpu_primitive(&polys[1]);
        gfx_draw_or_append_gpu_primitive(&polys[0]);
    }
}
