/*
 * Setter twin of battle_map_get_tile_data_value: stores one packed field of the
 * tile at (x, y, layer) and returns 0, or -2 for an unknown selector.
 * Selector 15 instead retries battle_clear_r2_2 up to 1000 times, raising
 * exception 0x80 on exhaustion, and returns its last result. The shifted
 * selectors reassign `value` first; an inline shift swaps the `or` operands
 * and breaks the target's shared or/sb tails.
 */
#include "fft/battle.h"
#include "psx/types.h"

s32 battle_map_set_tile_data_value(s32 field, s32 x, s32 y, s32 layer, s32 value) {
    map_tile_t* tile;
    s32 attempts;
    s32 result;

    attempts = 0;
    result = 0;
    tile = &g_battle_map_tile_data[(layer << 8) + y * g_map_max_x + x];
    switch (field) {
    case MAP_TILE_DATA_SURFACE_TYPE:
        tile->surface.value = (tile->surface.value & ~MAP_SURFACE_MASK) | (value & MAP_SURFACE_MASK);
        break;
    case MAP_TILE_DATA_UNKNOWN_01:
        tile->_unknown01 = value;
        break;
    case MAP_TILE_DATA_HEIGHT:
        tile->height = value;
        break;
    case MAP_TILE_DATA_HALF_HEIGHT:
        tile->depth_half_height = (tile->depth_half_height & MAP_TILE_DEPTH_MASK) | (value & MAP_TILE_HALF_HEIGHT_MASK);
        break;
    case MAP_TILE_DATA_DEPTH:
        value = (value & MAP_TILE_DEPTH_VALUE_MASK) << MAP_TILE_DEPTH_SHIFT;
        tile->depth_half_height = (tile->depth_half_height & MAP_TILE_HALF_HEIGHT_MASK) | value;
        break;
    case MAP_TILE_DATA_SLOPE_TYPE:
        tile->slope_type = value;
        break;
    case MAP_TILE_DATA_CEILING_DEPTH:
        tile->ceiling_depth_and_marks
            = (tile->ceiling_depth_and_marks & ~MAP_TILE_CEILING_DEPTH_MASK) | (value & MAP_TILE_CEILING_DEPTH_MASK);
        break;
    case MAP_TILE_DATA_BLOCKED:
        tile->flags_06.value = (tile->flags_06.value & ~MAP_TILE_FLAG_BLOCKED) | (value & MAP_TILE_FLAG_BLOCKED);
        break;
    case MAP_TILE_DATA_UNTARGETABLE:
        value = (value & 1) << MAP_TILE_FLAG_UNTARGETABLE_SHIFT;
        tile->flags_06.value = (tile->flags_06.value & ~MAP_TILE_FLAG_UNTARGETABLE) | value;
        break;
    case MAP_TILE_DATA_SHADOW_MODE:
        value = (value & MAP_TILE_SHADOW_MODE_VALUE_MASK) << MAP_TILE_SHADOW_MODE_SHIFT;
        tile->flags_06.value = (tile->flags_06.value & ~MAP_TILE_SHADOW_MODE_MASK) | value;
        break;
    case MAP_TILE_DATA_UNKNOWN_FLAGS_4_6:
        value = (value & 7) << 4;
        tile->flags_06.value = (tile->flags_06.value & 0x8F) | value;
        break;
    case MAP_TILE_DATA_FLAT_CAMERA_BLOCK_MASK:
        tile->camera_block_masks = (tile->camera_block_masks & MAP_TILE_CAMERA_BLOCK_STEEP_MASK)
            | (value & MAP_TILE_CAMERA_BLOCK_DIRECTION_MASK);
        break;
    case 15:
        while (attempts < 1000) {
            result = battle_return_zero_801842f8();
            if (result == 0) {
                break;
            }
            attempts++;
        }
        if (attempts == 1000) {
            main_system_handle_battle_load_exception(0x80);
        }
        break;
    default:
        result = -2;
        break;
    }
    return result;
}
