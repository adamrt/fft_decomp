#include "fft/battle.h"

/* The colour deltas are passed as sign-extended words; the u16 prototype
 * would zero-extend them, so this caller uses an all-s32 view.
 * The definition itself only matches with u16 colour parameters (s16 or s32
 * ones change its code), so the two views stay. */
#define MODIFY_PALETTE_WORDS ((void (*)(s32, s32, s32, s32, s32, s32, s32, s32))battle_map_modify_palette)

void battle_gfx_apply_misc_unit_palette_modulation(
    s32 preset_color, s32 time, s32 misc_id, s32 red, s32 green, s32 blue) {
    SVECTOR screen_coordinates;
    battle_unit_misc_data_t* unit;
    map_tile_t* tile;
    u16 red_copy;
    u16 green_copy;
    u16 blue_copy;

    unit = battle_unit_get_misc_data_by_misc_id(misc_id & 0xffff);
    red_copy = red;
    green_copy = green;
    blue_copy = blue;
    if (unit == 0) {
        return;
    }

    battle_unit_copy_misc_data_xyz_values(&screen_coordinates, unit);
    tile = battle_map_get_tile_data_pointer(screen_coordinates.vx / 28, screen_coordinates.vz / 28, unit->map_z);

    MODIFY_PALETTE_WORDS(preset_color, time, 3, misc_id, 0, (s16)red_copy, (s16)green_copy, (s16)blue_copy);

    if ((tile->depth_half_height & MAP_TILE_DEPTH_MASK) != 0
        && unit->screen.vy >= -(tile->height + (tile->depth_half_height >> MAP_TILE_DEPTH_SHIFT)) * 12) {
        MODIFY_PALETTE_WORDS(preset_color, time, 4, misc_id, 0, (s16)red_copy, (s16)green_copy, (s16)blue_copy);
    } else {
        MODIFY_PALETTE_WORDS(preset_color, time, 4, misc_id, 0, (s16)-red_copy, (s16)-green_copy, (s16)-blue_copy);
    }
}
