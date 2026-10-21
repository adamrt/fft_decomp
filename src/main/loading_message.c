/* The resident Now Loading message uses seven textured glyph quads per display
 * buffer. File loads and battle scene transitions link it into their ordering
 * tables, blinking it while they wait. */

#include "fft/gfx.h"
#include "fft/system.h"
#include "psx/libgpu.h"
#include "psx/types.h"

extern palette_t g_loading_palette;
static u32 s_loading_frame_counter;
extern u32 g_loading_visible;
extern POLY_FT4 g_loading_primitives[2][7];

/* Build the "Now Loading" message: seven glyph quads from texture page 0x1f with
 * the CLUT loaded to VRAM (0x70, 0x1e2), one copy per display buffer, the text
 * starting at (x + 0xa0, y + 0xc8).
 *
 * visible is stored for loading_otag_append, which blinks the
 * message, and the blink counter restarts. */
void loading_message_build(u32 visible, s32 x, s32 y) {
    POLY_FT4* prims = g_loading_primitives[0];
    POLY_FT4* poly;
    s32 i;

    g_loading_visible = visible;
    setPolyFT4(&prims[0]);
    s_loading_frame_counter = 0;
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

    poly = &g_loading_primitives[0][0];
    setXY4(poly, x + 0xA0, y + 0xC8, x + 0xAC, y + 0xC8, x + 0xA0, y + 0xD0, x + 0xAC, y + 0xD0);
    setUV4(poly, 0xE4, 0x9B, 0xF0, 0x9B, 0xE4, 0xA3, 0xF0, 0xA3);
    g_loading_primitives[1][0] = *poly;

    poly = &g_loading_primitives[0][1];
    setXY4(poly, x + 0xAB, y + 0xC8, x + 0xB2, y + 0xC8, x + 0xAB, y + 0xD0, x + 0xB2, y + 0xD0);
    setUV4(poly, 0xE4, 0xA3, 0xEB, 0xA3, 0xE4, 0xAB, 0xEB, 0xAB);
    g_loading_primitives[1][1] = *poly;

    poly = &g_loading_primitives[0][2];
    setXY4(poly, x + 0xB7, y + 0xC8, x + 0xC3, y + 0xC8, x + 0xB7, y + 0xD0, x + 0xC3, y + 0xD0);
    setUV4(poly, 0xAD, 0x55, 0xB9, 0x55, 0xAD, 0x5D, 0xB9, 0x5D);
    g_loading_primitives[1][2] = *poly;

    poly = &g_loading_primitives[0][3];
    setXY4(poly, x + 0xC3, y + 0xC8, x + 0xC8, y + 0xC8, x + 0xC3, y + 0xD0, x + 0xC8, y + 0xD0);
    setUV4(poly, 0xD1, 0x60, 0xD6, 0x60, 0xD1, 0x68, 0xD6, 0x68);
    g_loading_primitives[1][3] = *poly;

    poly = &g_loading_primitives[0][4];
    setXY4(poly, x + 0xC7, y + 0xC8, x + 0xCA, y + 0xC8, x + 0xC7, y + 0xD0, x + 0xCA, y + 0xD0);
    setUV4(poly, 0x7C, 0x52, 0x7F, 0x52, 0x7C, 0x5A, 0x7F, 0x5A);
    g_loading_primitives[1][4] = *poly;

    poly = &g_loading_primitives[0][5];
    setXY4(poly, x + 0xC9, y + 0xC8, x + 0xCE, y + 0xC8, x + 0xC9, y + 0xD0, x + 0xCE, y + 0xD0);
    setUV4(poly, 0xCE, 0xCC, 0xD3, 0xCC, 0xCE, 0xD4, 0xD3, 0xD4);
    g_loading_primitives[1][5] = *poly;

    poly = &g_loading_primitives[0][6];
    setXY4(poly, x + 0xCD, y + 0xCA, x + 0xD2, y + 0xCA, x + 0xCD, y + 0xD2, x + 0xD2, y + 0xD2);
    setUV4(poly, 0xEB, 0xA4, 0xF0, 0xA4, 0xEB, 0xAC, 0xF0, 0xAC);
    g_loading_primitives[1][6] = *poly;

    /* 16-color palette of the NOW LOADING glyph sheet, loaded to VRAM (0x70, 0x1e2). */
    LoadClut2((u32*)g_loading_palette, 0x70, 0x1e2);
}

/* Build the Now Loading message centered horizontally (x = 0x80). */
void loading_centered_message_build(u32 visible) {
    loading_message_build(visible, 0x80, 0);
}

/* Link the "Now Loading" quads of the current display buffer into otag while the
 * message is visible. It blinks: 60 calls shown, 60 calls hidden. */
void loading_otag_append(u32* otag) {
    int i;

    if (g_loading_visible != 0) {
        if (((s_loading_frame_counter / 60) & 1) == 0) {
            for (i = 0; i < 7; i++) {
                AddPrim(otag, &g_loading_primitives[g_gfx_screen_polarity][i]);
            }
        }
        s_loading_frame_counter++;
    }
}
