#include "fft/menu.h"

/* Initialize a menu quad off-screen at x=-512, with neutral shading,
 * a 4-bit texture page at VRAM (448, 0) and the menu CLUT at (960, 496). */
void menu_init_default_poly_ft4(POLY_FT4* poly) {
    SetPolyFT4(poly);
    SetShadeTex(poly, 0);
    setRGB0(poly, 0x80, 0x80, 0x80);
    setUV4(poly, 0, 0, 1, 0, 0, 1, 1, 1);
    setXY4(poly, -0x200, 0, -0x200, 0, -0x200, 0, -0x200, 0);
    poly->tpage = GetTPage(0, 0, 0x1c0, 0);
    poly->clut = getClut(0x3c0, 0x1f0);
}
