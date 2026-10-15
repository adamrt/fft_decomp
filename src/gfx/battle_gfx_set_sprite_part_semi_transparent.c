#include "fft/battle_gfx.h"

/* Set or clear bit 0 of a sprite part's flags: the part is drawn semi-transparent while it is set. */
void battle_gfx_set_sprite_part_semi_transparent(
    battle_gfx_sprite_display_data_t* display, s32 part_index, s32 enabled) {
    if (enabled != 0) {
        display->parts[part_index].flags |= BATTLE_SPRITE_DISPLAY_SEMI_TRANSPARENT;
    } else {
        display->parts[part_index].flags &= ~BATTLE_SPRITE_DISPLAY_SEMI_TRANSPARENT;
    }
}
