#include "fft/battle_gfx.h"

/* Replace the two mirror bits of a sprite part (2 mirrors x, 4 mirrors y). Never called. */
void battle_gfx_set_sprite_part_flip(battle_gfx_sprite_display_data_t* display, s32 part_index, u32 flags) {
    display->parts[part_index].flags = (display->parts[part_index].flags & 0xf9) | flags;
}
