#include "fft/battle.h"
#include "psx/gpu.h"

/* Initialize an array of BATTLE menu sprites with a shared CLUT. */
void battle_menu_init_sprite_array(SPRT* sprites, s32 count, s32 clut) {
    s32 index;

    for (index = 0; index < count; index++) {
        battle_menu_init_semitransparent_sprt(sprites);
        sprites->clut = clut;
        sprites++;
    }
}
