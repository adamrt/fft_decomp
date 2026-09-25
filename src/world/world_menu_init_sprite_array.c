#include "fft/world.h"
#include "psx/gpu.h"

/* Initialize an array of WORLD menu sprites with a shared CLUT. */
void world_menu_init_sprite_array(SPRT* sprites, s32 count, s32 clut) {
    s32 index;

    for (index = 0; index < count; index++) {
        world_menu_init_sprite(sprites);
        sprites->clut = clut;
        sprites++;
    }
}
