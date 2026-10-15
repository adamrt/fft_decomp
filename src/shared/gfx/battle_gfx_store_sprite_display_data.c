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
