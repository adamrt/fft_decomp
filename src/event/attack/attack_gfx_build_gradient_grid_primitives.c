#include "fft/event_attack.h"
#include "fft/gfx.h"

/* Build the 4x8 grid of shaded, textured quads behind the panel: each row is
 * one step brighter than the one above, and every cell shows the same 64x32
 * texture rectangle. */
void attack_gfx_build_gradient_grid_primitives(POLY_GT4* poly) {
    s32 row;
    s32 column;
    s32 intensity;

    if (g_attack_deploy_tiles_only_mode != 0) {
        return;
    }

    for (row = 0; row < 8; row++) {
        for (column = 0; column < 4; column++) {
            SetPolyGT4(poly);
            poly->clut = 0x7fbc;
            poly->tpage = GetTPage(0, 0, 0x3c0, 0x100);
            intensity = row * 8 + 0x40;
            setRGB0(poly, intensity, intensity, intensity);
            setRGB1(poly, intensity, intensity, intensity);
            intensity = (row + 1) * 8 + 0x40;
            setRGB2(poly, intensity, intensity, intensity);
            setRGB3(poly, intensity, intensity, intensity);
            setXY4(poly, column * 0x40 + 0x80, row * 0x20, column * 0x40 + 0xc0, row * 0x20, column * 0x40 + 0x80,
                row * 0x20 + 0x20, column * 0x40 + 0xc0, row * 0x20 + 0x20);
            setUV4(poly, 0x58, 0xd8, 0x98, 0xd8, 0x58, 0xf8, 0x98, 0xf8);
            SetSemiTrans(poly, 1);
            SetShadeTex(poly, 0);
            gfx_draw_or_append_gpu_primitive(poly);
            poly++;
        }
    }
}
