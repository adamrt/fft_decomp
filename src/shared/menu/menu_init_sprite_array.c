#include "fft/battle_menu.h"
#include "fft/menu.h"
#include "psx/libgpu.h"

/* Initialize an array of menu sprites with the scene's shading and a shared CLUT. */
void menu_init_sprite_array(SPRT* sprites, s32 count, s32 clut) {
    s32 index;

    for (index = 0; index < count; index++) {
        menu_init_sprt_variant(sprites);
        sprites->clut = clut;
        sprites++;
    }
}
