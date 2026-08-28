#include "fft/battle.h"
#include "fft/battle_move.h"
#include "fft/map.h"

/*
 * Returns the signed-16-bit screen Z at coords after applying unit height,
 * terrain depth, movement behavior and sprite-specific adjustments.
 * A unit-height record marked unavailable falls back to terrain projection.
 */
s32 battle_calculate_screen_z_from_input_coords(battle_unit_misc_data_t* unit, const battle_screen_coords_t* coords) {
    battle_unit_height_data_t height;
    map_tile_t* tile;
    s32 flags;
    s32 screen_z;
    s32 sprite;
    s32 animation;
    s32 raised;
    s32 adjusted;

    if (unit->current_unit_id_plus_one != 0) {
        battle_calculate_unit_height_data(&height, unit->current_unit_id_plus_one - 1);
        if (height.unit_flags & BATTLE_UNIT_HEIGHT_UNAVAILABLE) {
            screen_z = battle_map_calculate_slope_height(&coords->x, coords->z);
        } else {
            screen_z = -height.total_height / 2 * 12;
        }
    } else {
        screen_z = battle_map_calculate_slope_height(&coords->x, coords->z);
    }
    if (unit->battle_data != 0) {
        tile = battle_map_get_tile_data_pointer(coords->x / 28, coords->y / 28, coords->z);
        if (tile->depth_half_height & MAP_TILE_DEPTH_MASK) {
            flags = battle_move_get_effective_flags(unit->battle_data);
            sprite = unit->spritesheet_id;
            animation = unit->encoded_animation >> 1;
            if ((flags & BATTLE_EFFECTIVE_MOVEMENT_FLOAT) || (flags & BATTLE_EFFECTIVE_MOVEMENT_ON_WATER)) {
                screen_z -= (tile->depth_half_height >> MAP_TILE_DEPTH_SHIFT) * 12 + unit->depth_height_offset;
            } else if ((u32)(sprite - BATTLE_SPRITESHEET_ID_CHICKEN) < 2) {
                raised = screen_z + 4;
                screen_z
                    = raised - ((tile->depth_half_height >> MAP_TILE_DEPTH_SHIFT) * 12 + unit->depth_height_offset);
            } else if ((flags & BATTLE_EFFECTIVE_MOVEMENT_IN_WATER) || animation == 0x1a) {
                screen_z -= ((tile->depth_half_height >> MAP_TILE_DEPTH_SHIFT) - 1) * 12 + unit->depth_height_offset;
            } else if (sprite == BATTLE_SPRITESHEET_ID_TREASURE || sprite == BATTLE_SPRITESHEET_ID_CRYSTAL) {
                screen_z -= (tile->depth_half_height >> MAP_TILE_DEPTH_SHIFT) * 12 + unit->depth_height_offset;
            } else {
                goto subtract_offset; /* no depth adjustment: shared default tail */
            }
            return (s16)screen_z;
        }
    }

subtract_offset:
    adjusted = screen_z - unit->depth_height_offset;
    /* Keeps the target's subtract into a scratch register and copy to screen_z. */
    __asm__("" : "=r"(adjusted) : "0"(adjusted));
    screen_z = adjusted;
    return (s16)screen_z;
}
