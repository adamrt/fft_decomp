#include "fft/main_runtime.h"
#include "psx/types.h"

/* Primitive tag helpers (clean-room equivalents of the libgpu macros). */
#define setlen(p, l)  (((P_TAG*)(p))->len = (u8)(l))
#define setcode(p, c) (((P_TAG*)(p))->code = (u8)(c))
#define setPolyFT4(p) (setlen(p, 9), setcode(p, 0x2c))

void main_gfx_build_now_loading(u32 visible, s32 x, s32 y) {
    POLY_FT4* prims = g_main_gfx_now_loading_primitives[0];
    POLY_FT4* poly;
    s32 i;

    g_main_gfx_now_loading_visible = visible;
    setPolyFT4(&prims[0]);
    g_main_gfx_now_loading_frame_counter = 0;
    prims[0].r0 = 0x80;
    prims[0].g0 = 0x80;
    prims[0].b0 = 0x80;
    SetSemiTrans(&prims[0], 0);
    prims[0].tpage = 0x1f;
    prims[0].clut = 0x7887;
    for (i = 1; i < 7; i++) {
        prims[i] = prims[0];
    }
    /* Dead store, deleted by the compiler: a mention of prims after the copy
     * loop is what makes GCC hoist the block-move end bound (prims + 0x20)
     * out of the loop and allocate prims to s0. */
    poly = prims;

    poly = &g_main_gfx_now_loading_primitives[0][0];
    poly->x0 = x + 0xA0;
    poly->y0 = y + 0xC8;
    poly->x1 = x + 0xAC;
    poly->y1 = y + 0xC8;
    poly->x2 = x + 0xA0;
    poly->y2 = y + 0xD0;
    poly->x3 = x + 0xAC;
    poly->y3 = y + 0xD0;
    poly->u0 = 0xE4;
    poly->v0 = 0x9B;
    poly->u1 = 0xF0;
    poly->v1 = 0x9B;
    poly->u2 = 0xE4;
    poly->v2 = 0xA3;
    poly->u3 = 0xF0;
    poly->v3 = 0xA3;
    g_main_gfx_now_loading_primitives[1][0] = *poly;

    poly = &g_main_gfx_now_loading_primitives[0][1];
    poly->x0 = x + 0xAB;
    poly->y0 = y + 0xC8;
    poly->x1 = x + 0xB2;
    poly->y1 = y + 0xC8;
    poly->x2 = x + 0xAB;
    poly->y2 = y + 0xD0;
    poly->x3 = x + 0xB2;
    poly->y3 = y + 0xD0;
    poly->u0 = 0xE4;
    poly->v0 = 0xA3;
    poly->u1 = 0xEB;
    poly->v1 = 0xA3;
    poly->u2 = 0xE4;
    poly->v2 = 0xAB;
    poly->u3 = 0xEB;
    poly->v3 = 0xAB;
    g_main_gfx_now_loading_primitives[1][1] = *poly;

    poly = &g_main_gfx_now_loading_primitives[0][2];
    poly->x0 = x + 0xB7;
    poly->y0 = y + 0xC8;
    poly->x1 = x + 0xC3;
    poly->y1 = y + 0xC8;
    poly->x2 = x + 0xB7;
    poly->y2 = y + 0xD0;
    poly->x3 = x + 0xC3;
    poly->y3 = y + 0xD0;
    poly->u0 = 0xAD;
    poly->v0 = 0x55;
    poly->u1 = 0xB9;
    poly->v1 = 0x55;
    poly->u2 = 0xAD;
    poly->v2 = 0x5D;
    poly->u3 = 0xB9;
    poly->v3 = 0x5D;
    g_main_gfx_now_loading_primitives[1][2] = *poly;

    poly = &g_main_gfx_now_loading_primitives[0][3];
    poly->x0 = x + 0xC3;
    poly->y0 = y + 0xC8;
    poly->x1 = x + 0xC8;
    poly->y1 = y + 0xC8;
    poly->x2 = x + 0xC3;
    poly->y2 = y + 0xD0;
    poly->x3 = x + 0xC8;
    poly->y3 = y + 0xD0;
    poly->u0 = 0xD1;
    poly->v0 = 0x60;
    poly->u1 = 0xD6;
    poly->v1 = 0x60;
    poly->u2 = 0xD1;
    poly->v2 = 0x68;
    poly->u3 = 0xD6;
    poly->v3 = 0x68;
    g_main_gfx_now_loading_primitives[1][3] = *poly;

    poly = &g_main_gfx_now_loading_primitives[0][4];
    poly->x0 = x + 0xC7;
    poly->y0 = y + 0xC8;
    poly->x1 = x + 0xCA;
    poly->y1 = y + 0xC8;
    poly->x2 = x + 0xC7;
    poly->y2 = y + 0xD0;
    poly->x3 = x + 0xCA;
    poly->y3 = y + 0xD0;
    poly->u0 = 0x7C;
    poly->v0 = 0x52;
    poly->u1 = 0x7F;
    poly->v1 = 0x52;
    poly->u2 = 0x7C;
    poly->v2 = 0x5A;
    poly->u3 = 0x7F;
    poly->v3 = 0x5A;
    g_main_gfx_now_loading_primitives[1][4] = *poly;

    poly = &g_main_gfx_now_loading_primitives[0][5];
    poly->x0 = x + 0xC9;
    poly->y0 = y + 0xC8;
    poly->x1 = x + 0xCE;
    poly->y1 = y + 0xC8;
    poly->x2 = x + 0xC9;
    poly->y2 = y + 0xD0;
    poly->x3 = x + 0xCE;
    poly->y3 = y + 0xD0;
    poly->u0 = 0xCE;
    poly->v0 = 0xCC;
    poly->u1 = 0xD3;
    poly->v1 = 0xCC;
    poly->u2 = 0xCE;
    poly->v2 = 0xD4;
    poly->u3 = 0xD3;
    poly->v3 = 0xD4;
    g_main_gfx_now_loading_primitives[1][5] = *poly;

    poly = &g_main_gfx_now_loading_primitives[0][6];
    poly->x0 = x + 0xCD;
    poly->y0 = y + 0xCA;
    poly->x1 = x + 0xD2;
    poly->y1 = y + 0xCA;
    poly->x2 = x + 0xCD;
    poly->y2 = y + 0xD2;
    poly->x3 = x + 0xD2;
    poly->y3 = y + 0xD2;
    poly->u0 = 0xEB;
    poly->v0 = 0xA4;
    poly->u1 = 0xF0;
    poly->v1 = 0xA4;
    poly->u2 = 0xEB;
    poly->v2 = 0xAC;
    poly->u3 = 0xF0;
    poly->v3 = 0xAC;
    g_main_gfx_now_loading_primitives[1][6] = *poly;

    /* 16-colour CLUT of the NOW LOADING glyph sheet, loaded to VRAM (0x70, 0x1e2). */
    LoadClut2(g_main_gfx_now_loading_clut, 0x70, 0x1e2);
}
