/* Sprite part placement, flip and semi-transparency bits. */

#include "fft/battle_gfx.h"

/* Set part `index` of a sprite display: its place, the rectangle of the
 * texture page it shows and its battle_sprite_display_flags_e. */
void battle_gfx_store_sprite_display_data(battle_gfx_sprite_display_data_t* display, s32 index, s16 x_shift,
    s16 y_shift, s16 u, s16 v, s16 width, s16 height, s16 flags) {
    battle_gfx_sprite_part_display_data_t* part;

    part = &display->parts[index];
    part->x_shift = x_shift;
    part->y_shift = y_shift;
    part->u = u;
    part->v = v;
    part->width = width;
    part->height = height;
    part->flags = flags;
}

/* Replace the two mirror bits of a sprite part (2 mirrors x, 4 mirrors y). Never called. */
void battle_gfx_set_sprite_part_flip(battle_gfx_sprite_display_data_t* display, s32 part_index, u32 flags) {
    display->parts[part_index].flags = (display->parts[part_index].flags & 0xf9) | flags;
}

/* Set or clear bit 0 of a sprite part's flags: the part is drawn semi-transparent while it is set. */
void battle_gfx_set_sprite_part_semi_transparent(
    battle_gfx_sprite_display_data_t* display, s32 part_index, s32 enabled) {
    if (enabled != 0) {
        display->parts[part_index].flags |= BATTLE_SPRITE_DISPLAY_SEMI_TRANSPARENT;
    } else {
        display->parts[part_index].flags &= ~BATTLE_SPRITE_DISPLAY_SEMI_TRANSPARENT;
    }
}
