/* The resident Now Loading message uses seven textured glyph quads per display
 * buffer. File loads and battle scene transitions link it into their ordering
 * tables, blinking it while they wait. */

#include "fft/gfx/gfx.h"
#include "fft/sys/system.h"
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
 * visible is stored for loading_message_append_to_otag, which blinks the
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
    setXY4(poly, x + 160, y + 200, x + 172, y + 200, x + 160, y + 208, x + 172, y + 208);
    setUV4(poly, 228, 155, 240, 155, 228, 163, 240, 163);
    g_loading_primitives[1][0] = *poly;

    poly = &g_loading_primitives[0][1];
    setXY4(poly, x + 171, y + 200, x + 178, y + 200, x + 171, y + 208, x + 178, y + 208);
    setUV4(poly, 228, 163, 235, 163, 228, 171, 235, 171);
    g_loading_primitives[1][1] = *poly;

    poly = &g_loading_primitives[0][2];
    setXY4(poly, x + 183, y + 200, x + 195, y + 200, x + 183, y + 208, x + 195, y + 208);
    setUV4(poly, 173, 85, 185, 85, 173, 93, 185, 93);
    g_loading_primitives[1][2] = *poly;

    poly = &g_loading_primitives[0][3];
    setXY4(poly, x + 195, y + 200, x + 200, y + 200, x + 195, y + 208, x + 200, y + 208);
    setUV4(poly, 209, 96, 214, 96, 209, 104, 214, 104);
    g_loading_primitives[1][3] = *poly;

    poly = &g_loading_primitives[0][4];
    setXY4(poly, x + 199, y + 200, x + 202, y + 200, x + 199, y + 208, x + 202, y + 208);
    setUV4(poly, 124, 82, 127, 82, 124, 90, 127, 90);
    g_loading_primitives[1][4] = *poly;

    poly = &g_loading_primitives[0][5];
    setXY4(poly, x + 201, y + 200, x + 206, y + 200, x + 201, y + 208, x + 206, y + 208);
    setUV4(poly, 206, 204, 211, 204, 206, 212, 211, 212);
    g_loading_primitives[1][5] = *poly;

    poly = &g_loading_primitives[0][6];
    setXY4(poly, x + 205, y + 202, x + 210, y + 202, x + 205, y + 210, x + 210, y + 210);
    setUV4(poly, 235, 164, 240, 164, 235, 172, 240, 172);
    g_loading_primitives[1][6] = *poly;

    /* 16-color palette of the NOW LOADING glyph sheet, loaded to VRAM (0x70, 0x1e2). */
    LoadClut2((u32*)g_loading_palette, 0x70, 0x1e2);
}

/* Build the Now Loading message for BATTLE's screen: x = 0x80 is GFX_BATTLE_SCREEN_X,
 * BATTLE's draw-area origin. */
void loading_message_build_offset(u32 visible) {
    loading_message_build(visible, 0x80, 0);
}

/* Link the "Now Loading" quads of the current display buffer into otag while the
 * message is visible. It blinks: 60 calls shown, 60 calls hidden. */
void loading_message_append_to_otag(u32* otag) {
    int i;

    if (g_loading_visible != 0) {
        if (!((s_loading_frame_counter / 60) & 1)) {
            for (i = 0; i < 7; i++) {
                AddPrim(otag, &g_loading_primitives[g_gfx_screen_polarity][i]);
            }
        }
        s_loading_frame_counter++;
    }
}
