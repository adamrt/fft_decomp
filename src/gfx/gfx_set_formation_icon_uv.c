#include "fft/gfx.h"

/* Texture a quad with formation portrait `index`: a 31x48 cell from the 7
 * column sheet on the texture page at (0x100, 0), starting at v 0x28, with
 * the CLUT at x 0x100 + 16 * (index % 3), y index / 3. */
void gfx_set_formation_icon_uv(POLY_FT4* poly, s32 index) {
    s32 u = (index % 7) * 0x20;
    s32 v = (index / 7) * 0x30;

    setUV4(poly, u, v + 0x28, u + 0x1f, v + 0x28, u, v + 0x58, u + 0x1f, v + 0x58);
    poly->tpage = GetTPage(0, 0, 0x100, 0);
    poly->clut = GetClut(((index % 3) << 4) + 0x100, index / 3);
}
