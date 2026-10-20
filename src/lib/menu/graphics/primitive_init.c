/* Default off-screen menu quads and sprites, initialized before a window supplies
 * its image, placement and palette. */

#include "fft/menu_gfx.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Empty in BATTLE and WORLD; menu_subsystems_init calls it. */
void menu_init_stub(void) {
}

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

/* Initialize the fixed sprite of the menu graphics; only BATTLE shades its texture. */
void menu_init_sprt(SPRT* sprite) {
    SetSprt(sprite);
#ifdef FFT_BATTLE
    SetShadeTex(sprite, 1);
#else
    SetShadeTex(sprite, 0);
#endif
    setRGB0(sprite, 0x80, 0x80, 0x80);
    sprite->x0 = 0x200;
    sprite->u0 = 0;
    sprite->v0 = 0;
    sprite->w = 0;
    sprite->h = 0;
    sprite->y0 = 0;
    sprite->clut = MENU_CLUT_NORMAL;
    SetSemiTrans(sprite, 1);
}

/* menu_init_sprt for count consecutive sprites, each then given clut. */
void menu_init_sprite_array(SPRT* sprites, s32 count, s32 clut) {
    s32 index;

    for (index = 0; index < count; index++) {
        menu_init_sprt(sprites);
        sprites->clut = clut;
        sprites++;
    }
}
