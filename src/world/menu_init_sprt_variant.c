#include "fft/world.h"
#include "psx/libgpu.h"

/* Initialize the fixed sprite used by WORLD menu graphics. */
void menu_init_sprt_variant(SPRT* sprite) {
    SetSprt(sprite);
    SetShadeTex(sprite, 0);
    setRGB0(sprite, 0x80, 0x80, 0x80);
    sprite->x0 = 0x200;
    sprite->u0 = 0;
    sprite->v0 = 0;
    sprite->w = 0;
    sprite->h = 0;
    sprite->y0 = 0;
    sprite->clut = 0x7c3c;
    SetSemiTrans(sprite, 1);
}
