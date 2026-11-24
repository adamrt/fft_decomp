/* The panel background's 4x8 grid of shaded textured quads, each row brighter than the one above. */

#include "fft/gfx/gfx_primitive.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Build and submit the panel background's 4x8 gradient grid. */
void gfx_build_gradient_grid_primitives(POLY_GT4* poly) {
    s32 row;
    s32 column;
    s32 intensity;

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
            setXY4(poly, column * 64 + 128, row * 32, column * 64 + 192, row * 32, column * 64 + 128, row * 32 + 32,
                column * 64 + 192, row * 32 + 32);
            setUV4(poly, 88, 216, 152, 216, 88, 248, 152, 248);
            SetSemiTrans(poly, 1);
            SetShadeTex(poly, 0);
            gfx_draw_or_append_gpu_primitive(poly);
            poly++;
        }
    }
}
