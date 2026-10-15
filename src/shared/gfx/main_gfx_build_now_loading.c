#include "fft/main.h"
#include "psx/types.h"

/* Build the "Now Loading" message: seven glyph quads from texture page 0x1f with
 * the CLUT loaded to VRAM (0x70, 0x1e2), one copy per display buffer, the text
 * starting at (x + 0xa0, y + 0xc8).
 *
 * visible is stored for main_gfx_add_now_loading_to_otag, which blinks the
 * message, and the blink counter restarts. */
void main_gfx_build_now_loading(u32 visible, s32 x, s32 y) {
    POLY_FT4* prims = g_main_gfx_now_loading_primitives[0];
    POLY_FT4* poly;
    s32 i;

    g_main_gfx_now_loading_visible = visible;
    setPolyFT4(&prims[0]);
    g_main_gfx_now_loading_frame_counter = 0;
    setRGB0(&prims[0], 0x80, 0x80, 0x80);
    SetSemiTrans(&prims[0], 0);
    prims[0].tpage = 0x1f;
    prims[0].clut = getClut(0x70, 0x1e2);
    for (i = 1; i < 7; i++) {
        prims[i] = prims[0];
    }
    /* Dead store, deleted by the compiler: a mention of prims after the copy
     * loop is what makes GCC hoist the block-move end bound (prims + 0x20)
     * out of the loop and allocate prims to s0. */
    poly = prims;

    poly = &g_main_gfx_now_loading_primitives[0][0];
    setXY4(poly, x + 0xA0, y + 0xC8, x + 0xAC, y + 0xC8, x + 0xA0, y + 0xD0, x + 0xAC, y + 0xD0);
    setUV4(poly, 0xE4, 0x9B, 0xF0, 0x9B, 0xE4, 0xA3, 0xF0, 0xA3);
    g_main_gfx_now_loading_primitives[1][0] = *poly;

    poly = &g_main_gfx_now_loading_primitives[0][1];
    setXY4(poly, x + 0xAB, y + 0xC8, x + 0xB2, y + 0xC8, x + 0xAB, y + 0xD0, x + 0xB2, y + 0xD0);
    setUV4(poly, 0xE4, 0xA3, 0xEB, 0xA3, 0xE4, 0xAB, 0xEB, 0xAB);
    g_main_gfx_now_loading_primitives[1][1] = *poly;

    poly = &g_main_gfx_now_loading_primitives[0][2];
    setXY4(poly, x + 0xB7, y + 0xC8, x + 0xC3, y + 0xC8, x + 0xB7, y + 0xD0, x + 0xC3, y + 0xD0);
    setUV4(poly, 0xAD, 0x55, 0xB9, 0x55, 0xAD, 0x5D, 0xB9, 0x5D);
    g_main_gfx_now_loading_primitives[1][2] = *poly;

    poly = &g_main_gfx_now_loading_primitives[0][3];
    setXY4(poly, x + 0xC3, y + 0xC8, x + 0xC8, y + 0xC8, x + 0xC3, y + 0xD0, x + 0xC8, y + 0xD0);
    setUV4(poly, 0xD1, 0x60, 0xD6, 0x60, 0xD1, 0x68, 0xD6, 0x68);
    g_main_gfx_now_loading_primitives[1][3] = *poly;

    poly = &g_main_gfx_now_loading_primitives[0][4];
    setXY4(poly, x + 0xC7, y + 0xC8, x + 0xCA, y + 0xC8, x + 0xC7, y + 0xD0, x + 0xCA, y + 0xD0);
    setUV4(poly, 0x7C, 0x52, 0x7F, 0x52, 0x7C, 0x5A, 0x7F, 0x5A);
    g_main_gfx_now_loading_primitives[1][4] = *poly;

    poly = &g_main_gfx_now_loading_primitives[0][5];
    setXY4(poly, x + 0xC9, y + 0xC8, x + 0xCE, y + 0xC8, x + 0xC9, y + 0xD0, x + 0xCE, y + 0xD0);
    setUV4(poly, 0xCE, 0xCC, 0xD3, 0xCC, 0xCE, 0xD4, 0xD3, 0xD4);
    g_main_gfx_now_loading_primitives[1][5] = *poly;

    poly = &g_main_gfx_now_loading_primitives[0][6];
    setXY4(poly, x + 0xCD, y + 0xCA, x + 0xD2, y + 0xCA, x + 0xCD, y + 0xD2, x + 0xD2, y + 0xD2);
    setUV4(poly, 0xEB, 0xA4, 0xF0, 0xA4, 0xEB, 0xAC, 0xF0, 0xAC);
    g_main_gfx_now_loading_primitives[1][6] = *poly;

    /* 16-color palette of the NOW LOADING glyph sheet, loaded to VRAM (0x70, 0x1e2). */
    LoadClut2((u32*)g_main_gfx_now_loading_palette, 0x70, 0x1e2);
}
