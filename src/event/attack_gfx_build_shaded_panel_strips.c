#include "fft/attack.h"
#include "fft/battle_gfx.h"
#include "fft/thread.h"

/* libgpu-style rectangle setters; the macro argument re-evaluation and the
 * narrowing store into each field are part of the matched expression shapes. */
#define SET_UVWH(prim, _u0, _v0, _w, _h)                                                                               \
    ((prim)->u0 = (_u0), (prim)->v0 = (_v0), (prim)->u1 = (_u0) + (_w), (prim)->v1 = (_v0), (prim)->u2 = (_u0),        \
        (prim)->v2 = (_v0) + (_h), (prim)->u3 = (_u0) + (_w), (prim)->v3 = (_v0) + (_h))
#define SET_XYWH(prim, _x0, _y0, _w, _h)                                                                               \
    ((prim)->x0 = (_x0), (prim)->y0 = (_y0), (prim)->x1 = (_x0) + (_w), (prim)->y1 = (_y0), (prim)->x2 = (_x0),        \
        (prim)->y2 = (_y0) + (_h), (prim)->x3 = (_x0) + (_w), (prim)->y3 = (_y0) + (_h))
#define SET_RGB(prim, n, value) ((prim)->r##n = (value), (prim)->g##n = (value), (prim)->b##n = (value))

/*
 * Builds and submits a 2x2 set of shaded POLY_GT4 panel strips at the current
 * thread's position.
 *
 * Row 0 is the solid part of the strip and row 1 a narrow edge whose shade
 * fades to 0; `reverse` selects which side of `width` is drawn. Column 0 is
 * the same quad offset by one pixel with a different blend mode (provisional
 * reading: a drop shadow).
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

    x = battle_thread_get_current_parameter_1();
    y = battle_thread_get_current_parameter_2();
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
                    SET_UVWH(poly, 0, 0xe0, width, 0x14);
                    SET_XYWH(poly, x + 0x80 + dx, y + 0x60 + dy, width, height);
                } else {
                    rest = 0xff - width;
                    SET_UVWH(poly, width, 0xe0, rest, 0x14);
                    SET_XYWH(poly, x + (0x80 + width) + dx, y + 0x60 + dy, rest, height);
                }
                SET_RGB(poly, 0, shade);
                SET_RGB(poly, 1, shade);
                SET_RGB(poly, 2, shade);
                SET_RGB(poly, 3, shade);
            } else {
                if (reverse == 0) {
                    SET_RGB(poly, 0, shade);
                    SET_RGB(poly, 1, 0);
                    SET_RGB(poly, 2, shade);
                    SET_RGB(poly, 3, 0);
                } else {
                    SET_RGB(poly, 1, shade);
                    SET_RGB(poly, 0, 0);
                    SET_RGB(poly, 3, shade);
                    SET_RGB(poly, 2, 0);
                }
                if (reverse == 0) {
                    edge = 0x20;
                    if (width >= 0xdf) {
                        edge = 0xfe - width;
                    }
                    SET_UVWH(poly, width, 0xe0, edge, 0x14);
                    SET_XYWH(poly, x + 0x80 + width + dx, y + 0x60 + dy, edge, height);
                } else {
                    edge = 0x20;
                    if (width < 0x20) {
                        edge = width;
                    }
                    SET_UVWH(poly, width - edge, 0xe0, edge, 0x14);
                    SET_XYWH(poly, x + (0x80 + width) - edge + dx, y + 0x60 + dy, edge, height);
                }
            }
            poly++;
        }
    }
    if (reverse != 0) {
        battle_gfx_draw_or_append_gpu_primitive((s32*)&polys[1]);
        battle_gfx_draw_or_append_gpu_primitive((s32*)&polys[0]);
        battle_gfx_draw_or_append_gpu_primitive((s32*)&polys[3]);
        battle_gfx_draw_or_append_gpu_primitive((s32*)&polys[2]);
    } else {
        battle_gfx_draw_or_append_gpu_primitive((s32*)&polys[3]);
        battle_gfx_draw_or_append_gpu_primitive((s32*)&polys[2]);
        battle_gfx_draw_or_append_gpu_primitive((s32*)&polys[1]);
        battle_gfx_draw_or_append_gpu_primitive((s32*)&polys[0]);
    }
}
