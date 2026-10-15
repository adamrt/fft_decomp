#include "fft/world.h"
#include "psx/libgpu.h"

/* Set up the two menu sprites for one record. The first sprite is positioned
 * from the record's x/y according to its layout mode (1 or 2), the second is
 * a fixed 16x16 sprite. Both use CLUT 0x7d7c.
 *
 * `mode` is reused to carry the y coordinate once the mode test is done; that
 * reuse is what puts the y value in the target's v1. */
void world_menu_build_layout_sprites(menu_entry_t* entry, SPRT sprite[2], s32 unused_2) {
    u16 x;
    s32 mode;

    menu_init_sprt_variant(sprite);
    sprite->clut = 0x7D7C;
    mode = entry->header_id;
    if (mode == 1) {
        x = entry->window.x;
        sprite->x0 = x;
        mode = (u16)entry->window.y;
        sprite->u0 = 0xA8;
        sprite->v0 = 0x88;
        sprite->w = 0x28;
        sprite->h = 8;
        mode -= 1;
        sprite->y0 = mode;
    } else {
        if (mode == 2) {
            x = entry->window.x;
            sprite->x0 = x - 6;
            mode = (u16)entry->window.y;
            sprite->u0 = 0xA8;
            sprite->v0 = 0x78;
            sprite->w = 0x28;
            sprite->h = 0x10;
            mode -= 6;
            sprite->y0 = mode;
        }
    }
    sprite++;
    menu_init_sprt_variant(sprite);
    sprite->w = 0x10;
    sprite->h = 0x10;
    sprite->clut = 0x7D7C;
}
